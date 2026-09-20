#include <memory>

#include <Backend/Contracts/Audio/AudioExtensions.hpp>
#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Contracts/Audio/AudioBackendId.hpp>
#include <Backend/Contracts/Render/RenderBackendId.hpp>
#include <Backend/Runtime/BackendRuntime.hpp>
#include <Event/Input.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Effects/DopplerEffect.hpp>
#include <Media/Audio/Resources/AudioClipLoader.hpp>
#include <Debugger/Overlay.hpp>
#include <Debugger/LogDebugger.hpp>
#include <Window/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>

namespace {

constexpr auto kAudioPath = R"(C:\Users\Romi\Downloads\1.mp3)";

class ProbeScreen final : public atom::Screen {
    public:
        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::render::Color{24, 24, 32});
        }

        auto Update(float) -> void override {}

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type != atom::window::EventType::KeyPressed)
                return false;
            if (std::get<atom::window::KeyEvent>(event.data).key != atom::event::Key::Escape)
                return false;
            atom::RenderWindow::GetInstance().Shutdown();
            return true;
        }
};

class ProbeDebugger final : public atom::debugger::DebugPanel {
    public:
        auto LoadSource() -> void {
            source_.reset();
            auto& runtime = atom::backend::BackendRuntime::GetInstance();
            atom::AudioClipLoader loader{runtime.AudioDecoders()};
            const auto decoded = loader.Load(kAudioPath);
            if (!decoded)
                return;
            // AcquireAudioBackend(): hold a strong reference while the source is
            // created, so a backend switch on another thread cannot destroy the
            // backend mid-call. The runtime never throws when nothing is active.
            if (const auto backend = runtime.AcquireAudioBackend())
                source_ = backend->CreateSFXSource(decoded->pcm, decoded->spec);
        }

        // Backend switching is a "release first, then switch" operation: a source
        // belongs to the backend that created it, and destroying an SDL3 backend
        // tears the whole audio subsystem (and every stream in it) down. The
        // runtime also detaches leftover sources, but an owner should not rely on
        // that safety net.
        auto SwitchBackend(const atom::backend::AudioBackendId id) -> void {
            auto& runtime = atom::backend::BackendRuntime::GetInstance();
            source_.reset();
            if (!runtime.SetAudioBackend(id))
                return;
            LOG_INFO(atom::log::core::Screen, "Probe reloading the source on backend '" +
                                                  runtime.GetAudioBackendId() + "' (generation " +
                                                  std::to_string(runtime.GetAudioBackendGeneration()) + ")");
            LoadSource();
        }

    protected:
        auto OnDrawOverlay() -> void override {
            auto& runtime = atom::backend::BackendRuntime::GetInstance();
            ImGui::Begin("Spatial Audio Backend Probe");
            ImGui::Text("Active backend: %s (generation %llu)", runtime.GetAudioBackendId().c_str(),
                        static_cast<unsigned long long>(runtime.GetAudioBackendGeneration()));
            if (ImGui::Button("Use native SDL3"))
                SwitchBackend(atom::backend::AudioBackendId::Sdl3);
            ImGui::SameLine();
            if (ImGui::Button("Use SDL3_mixer"))
                SwitchBackend(atom::backend::AudioBackendId::Sdl3Mixer);

            if (!source_) {
                ImGui::TextDisabled("No source. Check kAudioPath and reload.");
                if (ImGui::Button("Reload"))
                    LoadSource();
                ImGui::End();
                return;
            }

            DrawCapabilities();
            DrawPlaybackControls();
            DrawDopplerPlugin();
            if (!doppler_enabled_)
                DrawManualSpatialControls();

            ImGui::Text("State: %d", static_cast<int>(source_->GetState()));
            ImGui::TextDisabled("ESC exits. High-frequency spatial updates are intentionally not logged.");
            ImGui::End();
        }

    private:
        auto DrawCapabilities() const -> void {
            const bool pitch = dynamic_cast<atom::audio::IPitchControl*>(source_.get()) != nullptr;
            const bool pan = dynamic_cast<atom::audio::IPanControl*>(source_.get()) != nullptr;
            const bool spatial = dynamic_cast<atom::audio::ISpatialEmitter*>(source_.get()) != nullptr;
            ImGui::Text("Pitch: %s | Pan: %s | 3D position: %s", pitch ? "available" : "fallback",
                        pan ? "available" : "fallback", spatial ? "available" : "fallback");
        }

        auto DrawPlaybackControls() -> void {
            if (ImGui::Button("Play once")) {
                source_->SetLooping(false);
                source_->Play();
            }
            ImGui::SameLine();
            if (ImGui::Button("Play loop")) {
                source_->SetLooping(true);
                source_->Play();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                source_->Stop();

            if (ImGui::Checkbox("Loop", &loop_))
                source_->SetLooping(loop_);
            if (ImGui::SliderFloat("Volume", &volume_, 0.0f, 100.0f))
                source_->SetVolume(volume_);
        }

        auto DrawDopplerPlugin() -> void {
            ImGui::SeparatorText("Doppler effect plugin");
            ImGui::Checkbox("Enable Doppler effect", &doppler_enabled_);
            ImGui::TextWrapped("The plugin calculates pitch from relative radial velocity. With SDL3_mixer, 3D position "
                               "also supplies left/right spatial panning from the relative X coordinate.");

            ImGui::SliderFloat3("Listener position (X, Y, Z)", &listener_.position.x, -20.0f, 20.0f);
            ImGui::SliderFloat3("Listener velocity (X, Y, Z)", &listener_.velocity.x, -100.0f, 100.0f);
            ImGui::SliderFloat3("Emitter position (X, Y, Z)", &emitter_.position.x, -20.0f, 20.0f);
            ImGui::SliderFloat3("Emitter velocity (X, Y, Z)", &emitter_.velocity.x, -100.0f, 100.0f);
            ImGui::TextDisabled("X: negative is left, positive is right. Y: down/up. Z: forward/back.");

            ImGui::SliderFloat("Speed of sound", &settings_.speed_of_sound, 1.0f, 1000.0f, "%.1f units/s");
            ImGui::SliderFloat("Minimum pitch ratio", &settings_.min_pitch_ratio, 0.01f, 2.0f);
            ImGui::SliderFloat("Maximum pitch ratio", &settings_.max_pitch_ratio, 0.01f, 4.0f);
            ImGui::SliderFloat("Pan fallback distance", &settings_.pan_distance, 0.1f, 50.0f, "%.1f units");
            ImGui::Checkbox("Apply 3D position when available", &settings_.apply_3d_position);
            ImGui::Checkbox("Use stereo-pan fallback", &settings_.allow_stereo_pan_fallback);

            doppler_.SetListener(listener_);
            doppler_.SetEmitter(emitter_);
            doppler_.SetSettings(settings_);
            if (!doppler_enabled_)
                return;

            doppler_.Apply(*source_);
            const auto& result = doppler_.GetLastResult();
            ImGui::Text("Pitch ratio: %.3f | Distance: %.2f", result.pitch_ratio, result.distance);
            ImGui::Text("Listener radial velocity: %.2f | Emitter radial velocity: %.2f",
                        result.listener_radial_velocity, result.emitter_radial_velocity);
            ImGui::Text("Calculated stereo pan: %.3f", result.stereo_pan);
            ImGui::Text("Pitch applied: %s | Panning: %s", result.pitch_applied ? "yes" : "fallback",
                        GetPanningModeName(result.panning_mode));
        }

        auto DrawManualSpatialControls() -> void {
            ImGui::SeparatorText("Manual spatial controls");
            ImGui::TextWrapped("Pan is one left/right value, not three coordinates: -1 is fully left, 0 is centre, "
                               "+1 is fully right. SDL3_mixer treats manual pan and 3D position as mutually exclusive.");
            ImGui::SliderFloat("Manual pitch ratio", &manual_pitch_, 0.25f, 2.0f);
            ImGui::SliderFloat("Manual stereo pan", &manual_pan_, -1.0f, 1.0f);
            ImGui::Checkbox("Enable manual 3D position", &manual_position_enabled_);
            ImGui::SliderFloat3("Manual 3D position (X, Y, Z)", &manual_position_.x, -10.0f, 10.0f);
            ImGui::TextDisabled("X: left/right. Y: down/up. Z: forward/back.");

            const bool pitch_applied = atom::audio::TrySetPitch(*source_, manual_pitch_);
            const bool position_applied = manual_position_enabled_ &&
                                          atom::audio::TrySetPosition(*source_, manual_position_);
            const bool position_cleared = !manual_position_enabled_ && atom::audio::TryClearPosition(*source_);
            const bool pan_applied = !manual_position_enabled_ && atom::audio::TrySetPan(*source_, manual_pan_);
            ImGui::Text("Applied: pitch=%s, 3D position=%s, 3D clear=%s, manual pan=%s",
                        pitch_applied ? "yes" : "fallback", position_applied ? "yes" : "fallback",
                        position_cleared ? "yes" : "fallback", pan_applied ? "yes" : "fallback");
        }

        [[nodiscard]] static auto GetPanningModeName(const atom::audio::DopplerPanningMode mode) -> const char* {
            switch (mode) {
            case atom::audio::DopplerPanningMode::Position3D:
                return "3D position";
            case atom::audio::DopplerPanningMode::StereoPanFallback:
                return "stereo-pan fallback";
            case atom::audio::DopplerPanningMode::None:
                return "unavailable";
            }
            return "unknown";
        }

        std::unique_ptr<atom::audio::IAudioSource> source_;
        atom::audio::DopplerEffect doppler_;
        atom::audio::SpatialMotion listener_{};
        atom::audio::SpatialMotion emitter_{.position = {-6.0f, 0.0f, 0.0f}};
        atom::audio::DopplerSettings settings_{};
        atom::audio::AudioPosition manual_position_{};
        float volume_ = 100.0f;
        float manual_pitch_ = 1.0f;
        float manual_pan_ = 0.0f;
        bool loop_ = false;
        bool doppler_enabled_ = false;
        bool manual_position_enabled_ = false;
};

} // namespace

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);
    atom::ScreenManager::GetInstance().LoadScreen("Probe", std::make_unique<ProbeScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("Probe");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom - Spatial Audio Backend Probe", atom::algo::Vec2{860, 780},
                      atom::backend::RenderBackendId::SdlGpu);
    ProbeDebugger debugger;
    debugger.LoadSource();
    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);
    debugger.Attach(window);
    window.Run();
}

/**
  * @file           : SFXPlayback.cpp
  * @author         : Romi Brooks
  * @brief          : SFX playback demo with voice pool (overlapping sounds).
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <functional>
#include <string>
#include <utility>

#include <Platform/ApplicationEntry.hpp>
#include <Media/Audio/AudioBackend.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/SFXPlayer.hpp>
#include <Media/Audio/Resources/AudioClipCache.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>
#include <Runtime/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Runtime/Application.hpp>
#include <Runtime/Screen.hpp>
#include <Debugger/Overlay.hpp>
#include <Debugger/Panels/LogDebugger.hpp>

#include <Log/LogSystem.hpp>

namespace {
// replace it
constexpr auto SFXDir = R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\)";
constexpr auto SFX1Name = "Cymatics - Vocal Essentials One Shot 1 - C.wav";
constexpr auto SFX2Name = "Cymatics - Vocal Essentials One Shot 2 - C.wav";

// Debugger overlay
class SFXDebugger final : public atom::debugger::DebugPanel {
    public:
        // reload_clips is what makes a backend switch usable: the runtime drops
        // every source before the swap, and SFXPlayer's reset also clears the
        // decoded PCM cache. See DrawBackendControls().
        SFXDebugger(atom::SFXPlayer& sfx, std::function<bool()> reload_clips)
            : DebugPanel("SFXDebugger"), sfx_(sfx), reload_clips_(std::move(reload_clips)) {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("SFX Debugger");
            ImGui::Text("Press A to play sfx1, B to play sfx2");
            if (ImGui::Button("A")) {
                sfx_.Play("registerId_1");
            }
            if (ImGui::Button("B")) {
                sfx_.Play("registerId_2");
            }
            ImGui::Separator();

            ImGui::Text("this btm allows you play those file at the same time:");
            if (ImGui::Button("Play")) {
                sfx_.Play("registerId_1");
                sfx_.Play("registerId_2");
            }
            ImGui::Separator();

            DrawBackendControls();
            ImGui::Separator();

            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }

    private:
        // Hot backend switching. SetAudioBackend() builds the replacement first
        // and only then drops the old backend, so a failed switch keeps the
        // current backend and its sources alive. On success every registered
        // player resets, and because SFXPlayer::Reset() clears the decoded PCM
        // cache, both clips have to be decoded again here.
        auto SwitchBackend(const atom::backend::AudioBackendId id) -> void {
            const auto generation = atom::audio::GetAudioBackendGeneration();
            if (!atom::audio::SetAudioBackend(id))
                return;
            // Selecting the already-active backend does not reset anything;
            // reloading would only re-insert the clips that are still cached.
            if (atom::audio::GetAudioBackendGeneration() == generation)
                return;

            LOG_INFO(atom::log::audio::Sfx, "Audio backend switched to '" + atom::audio::GetAudioBackendId() +
                                                "' (generation " +
                                                std::to_string(atom::audio::GetAudioBackendGeneration()) + ")");
            reload_clips_();
        }

        auto DrawBackendControls() -> void {
            ImGui::SeparatorText("Audio backend");
            ImGui::Text("Active backend: %s (generation %llu)", atom::audio::GetAudioBackendId().c_str(),
                        static_cast<unsigned long long>(atom::audio::GetAudioBackendGeneration()));
            ImGui::Text("Clips loaded: %zu / 2", sfx_.GetLoadedCount());

            if (ImGui::Button("Use native SDL3"))
                SwitchBackend(atom::backend::AudioBackendId::Sdl3);
            ImGui::SameLine();
            if (ImGui::Button("Use SDL3_mixer"))
                SwitchBackend(atom::backend::AudioBackendId::Sdl3Mixer);
            ImGui::TextWrapped("A switch destroys the old backend and its voice pools, and clears the decoded clip "
                               "cache. This demo re-decodes both files and is ready to play again immediately.");
        }

        atom::SFXPlayer& sfx_;
        std::function<bool()> reload_clips_;
};

class SFXScreen final : public atom::Screen {
    public:
        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::render::Color{30, 30, 60});
        }

        // INPUT: two paths exist and they are not interchangeable.
        //
        //   This one (event style) - a discrete command that must fire once per
        //   press and wants the exact event payload, here "Escape went down".
        //   Returning true consumes the event.
        //
        //   input::InputSystem (polling style) - anything continuous: "is this
        //   key held", "how far did the mouse move", "was a chord pressed this
        //   frame", and the only way to ask the same question from more than one
        //   place in a frame. Add <Input/InputSystem.hpp> and read it from
        //   Update()/FixedUpdate().
        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.key == atom::input::Key::Escape) {
                    atom::RenderWindow::GetInstance().RequestClose();
                    return true;
                }
            }
            return false;
        }

        auto Update(float) -> void override {}
};
} // namespace

auto atom::platform::ApplicationMain(int, char**) -> int {
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);
    atom::AudioMixer mixer;
    atom::AudioClipCache clips;
    atom::SFXPlayer sfx{clips, mixer};

    // Mount the SFX directory as res:// so clips load through the VFS contract.
    std::unique_ptr<atom::fs::NativeFileSystem> filesystem{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(SFXDir), filesystem) !=
            atom::fs::Result::Success ||
        !filesystem) {
        LOG_ERROR(atom::log::audio::Music, "SFX directory unavailable: " + std::string{SFXDir});
        return 1;
    }
    // Parse() rather than TryParse(): the failure reason matters here, because the
    // message is the only thing that tells the reader which filename was rejected
    // and why.
    const auto sfx1 = atom::fs::AssetPath::Parse("res://" + std::string{SFX1Name});
    const auto sfx2 = atom::fs::AssetPath::Parse("res://" + std::string{SFX2Name});
    if (!sfx1) {
        LOG_ERROR(atom::log::audio::Music, "SFX1 is not a valid asset path: " + sfx1.error().Message());
        return 1;
    }
    if (!sfx2) {
        LOG_ERROR(atom::log::audio::Music, "SFX2 is not a valid asset path: " + sfx2.error().Message());
        return 1;
    }

    // A clip that fails to load is a hard failure before the window opens: the
    // screen below triggers these ids, and silently continuing would show a demo
    // where the keys do nothing.
    if (!sfx.Load("registerId_1", *filesystem, *sfx1)) {
        LOG_ERROR(atom::log::audio::Music, "Could not load " + std::string{SFX1Name});
        return 1;
    }
    if (!sfx.Load("registerId_2", *filesystem, *sfx2)) {
        LOG_ERROR(atom::log::audio::Music, "Could not load " + std::string{SFX2Name});
        return 1;
    }

    atom::ScreenManager::GetInstance().LoadScreen("SFX", std::make_unique<SFXScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("SFX");

    auto& window = atom::RenderWindow::GetInstance();
    auto& application = atom::runtime::Application::GetInstance();
    const auto initialized = application.Initialize("Atom Engine - SFX Playback Example", atom::algo::Vec2{720, 720},
                                              atom::backend::RenderBackendId::SdlGpu);
    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    // Backend switches reset the SFXPlayer and clear its decoded clip cache (see
    // SFXDebugger::SwitchBackend), so both clips are decoded again on demand.
    const auto reload_clips = [&sfx, &filesystem, &sfx1, &sfx2]() {
        return sfx.Load("registerId_1", *filesystem, *sfx1) &&
               sfx.Load("registerId_2", *filesystem, *sfx2);
    };

    SFXDebugger debugger{sfx, reload_clips};
    debugger.Attach(window);

    application.Run();
    return 0;
}

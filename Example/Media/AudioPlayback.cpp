/**
  * @file           : MusicPlayback.cpp
  * @author         : Romi Brooks
  * @brief          : Music playback demo with crossfade switching between two
  *                   tracks.
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
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Audio/Transitions/MusicCrossfade.hpp> // fade in plugin

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
constexpr auto MusicDir = R"(E:\Music\)";
constexpr auto Music1Name = "我的歌声里 - 曲婉婷.mp3";
constexpr auto Music2Name = "滴滴 - 覆予.mp3";

class MusicDebugger final : public atom::debugger::DebugPanel {
    public:
        // reload_tracks is what makes a backend switch usable: the runtime drops
        // every source before the swap, so the caller has to load the tracks
        // again against the new backend. See DrawBackendControls().
        MusicDebugger(atom::MusicPlayer& music, atom::audio::MusicCrossfade& fade,
                      std::function<bool()> reload_tracks)
            : DebugPanel("MusicDebugger"), music_(music), fade_(fade), reload_tracks_(std::move(reload_tracks)) {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("Music Debugger");

            ImGui::Text("Press A to play music1, B to play music2");

            constexpr float fade_time = 2.0f;

            if (ImGui::Button("A")) {
                if (playing_id_ != "registerId_1") {
                    fade_.Switch("registerId_1", fade_time);
                    playing_id_ = "registerId_1";
                }
            }

            if (ImGui::Button("B")) {
                if (playing_id_ != "registerId_2") {
                    fade_.Switch("registerId_2", fade_time);
                    playing_id_ = "registerId_2";
                }
            }

            ImGui::Text("If one of them is playing, switch it to the aim song");
            ImGui::Separator();

            DrawBackendControls();
            ImGui::Separator();

            // Seek / position. Everything comes from the MusicPlayer API; the
            // capability check keeps the UI honest on decoders that cannot seek.
            const auto now_playing = music_.GetNowPlaying();
            if (!now_playing.empty()) {
                const auto duration = music_.GetDuration(now_playing);
                auto position = music_.GetPlayingOffset(now_playing);
                if (duration > 0.0f)
                    ImGui::Text("Position: %.2f / %.2f s", static_cast<double>(position), static_cast<double>(duration));
                else
                    ImGui::Text("Position: %.2f s (duration unknown)", static_cast<double>(position));

                if (music_.IsSeekable(now_playing)) {
                    const auto slider_max = duration > 0.0f ? duration : position + 1.0f;
                    if (ImGui::SliderFloat("Seek", &position, 0.0f, slider_max, "%.2f s"))
                        music_.Seek(now_playing, position);
                    ImGui::SameLine();
                    if (ImGui::Button("-10s"))
                        music_.Seek(now_playing, position - 10.0f);
                    ImGui::SameLine();
                    if (ImGui::Button("+10s"))
                        music_.Seek(now_playing, position + 10.0f);
                } else {
                    ImGui::TextDisabled("The active decoder cannot seek this track");
                }
            } else {
                ImGui::TextDisabled("Seek: nothing is playing");
            }
            ImGui::Separator();

            ImGui::Text("this btm allows you play those file at the same time");
            if (ImGui::Button("Play")) {
                music_.Stop("registerId_1");
                music_.Stop("registerId_2");

                music_.Play("registerId_1");
                music_.Play("registerId_2");
            }
            ImGui::Separator();

            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }

    private:
        // Hot backend switching. This is the pattern every gameplay/settings
        // screen should follow:
        //   1. SetAudioBackend() creates the replacement backend first, so a
        //      failed switch leaves the current one (and its sources) intact.
        //   2. On success the runtime notifies every registered player, and
        //      MusicPlayer::OnAudioBackendChanging() calls Reset(): every track
        //      (and its decoder + VFS file handle) is destroyed.
        //   3. Playback is NOT migrated, so the owner reloads what it still
        //      needs against the new backend and resumes explicitly.
        auto SwitchBackend(const atom::backend::AudioBackendId id) -> void {
            const auto generation = atom::audio::GetAudioBackendGeneration();
            if (!atom::audio::SetAudioBackend(id))
                return;
            // Selecting the already-active backend is a no-op that does not
            // reset anything, so it must not reload either (Load() would skip
            // the already-loaded ids and the reload would be pointless).
            if (atom::audio::GetAudioBackendGeneration() == generation)
                return;

            LOG_INFO(atom::log::audio::Music, "Audio backend switched to '" + atom::audio::GetAudioBackendId() +
                                                  "' (generation " +
                                                  std::to_string(atom::audio::GetAudioBackendGeneration()) + ")");

            // Resume without the crossfade: its state points at sources that no
            // longer exist, and a fade would need the old backend alive.
            const std::string resume = playing_id_;
            fade_.Reset();
            playing_id_.clear();

            if (!reload_tracks_())
                return;
            if (!resume.empty()) {
                music_.Play(resume);
                playing_id_ = resume;
            }
        }

        auto DrawBackendControls() -> void {
            ImGui::SeparatorText("Audio backend");
            ImGui::Text("Active backend: %s (generation %llu)", atom::audio::GetAudioBackendId().c_str(),
                        static_cast<unsigned long long>(atom::audio::GetAudioBackendGeneration()));

            const int loaded = (music_.IsLoaded("registerId_1") ? 1 : 0) + (music_.IsLoaded("registerId_2") ? 1 : 0);
            ImGui::Text("Tracks loaded: %d / 2", loaded);

            if (ImGui::Button("Use native SDL3"))
                SwitchBackend(atom::backend::AudioBackendId::Sdl3);
            ImGui::SameLine();
            if (ImGui::Button("Use SDL3_mixer"))
                SwitchBackend(atom::backend::AudioBackendId::Sdl3Mixer);
            ImGui::TextWrapped("A switch destroys every source owned by the old backend, including the decoded "
                               "tracks. This demo reloads both files and resumes the track that was playing.");
        }

        atom::MusicPlayer& music_;
        atom::audio::MusicCrossfade& fade_;
        std::function<bool()> reload_tracks_;
        std::string playing_id_;
};

class MusicScreen final : public atom::Screen {
    public:
        explicit MusicScreen(atom::audio::MusicCrossfade& transition) : transition_(transition) {}

        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::render::Color{.r = 30, .g = 30, .b = 60});
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

        auto Update(const float delta_time) -> void override {
            transition_.Update(delta_time);
        }

    private:
        atom::audio::MusicCrossfade& transition_;
};
} // namespace

auto atom::platform::ApplicationMain(int, char**) -> int {
    // For windows: Console Debugger OUTPUT CP will be set to UTF-8
    atom::Log::SetConsoleOutputUtf8();

    // Log level
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};
    atom::audio::MusicCrossfade music_fade{music};

    // Mount the music directory as res:// so tracks load through the VFS
    // contract (same URI machinery as packaged assets) instead of a raw path.
    std::unique_ptr<atom::fs::NativeFileSystem> filesystem{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(MusicDir), filesystem) !=
            atom::fs::Result::Success ||
        !filesystem) {
        LOG_ERROR(atom::log::audio::Music, "Music directory unavailable: " + std::string{MusicDir});
        return 1;
    }
    // Parse() rather than TryParse(): the failure reason matters here, because the
    // message is the only thing that tells the reader which filename was rejected
    // and why.
    const auto music1 = atom::fs::AssetPath::Parse("res://" + std::string{Music1Name});
    const auto music2 = atom::fs::AssetPath::Parse("res://" + std::string{Music2Name});
    if (!music1) {
        LOG_ERROR(atom::log::audio::Music, "Music1 is not a valid asset path: " + music1.error().Message());
        return 1;
    }
    if (!music2) {
        LOG_ERROR(atom::log::audio::Music, "Music2 is not a valid asset path: " + music2.error().Message());
        return 1;
    }

    // A track that fails to load is a hard failure before the window opens: the
    // screen below plays these ids, and silently continuing would show a player
    // that does nothing.
    if (!music.Load("registerId_1", *filesystem, *music1)) {
        LOG_ERROR(atom::log::audio::Music, "Could not load " + std::string{Music1Name});
        return 1;
    }
    if (!music.Load("registerId_2", *filesystem, *music2)) {
        LOG_ERROR(atom::log::audio::Music, "Could not load " + std::string{Music2Name});
        return 1;
    }

    atom::ScreenManager::GetInstance().LoadScreen("MusicPlayback", std::make_unique<MusicScreen>(music_fade));
    atom::ScreenManager::GetInstance().SwitchScreen("MusicPlayback");

    auto& window = atom::RenderWindow::GetInstance();
    auto& application = atom::runtime::Application::GetInstance();
    const auto initialized = application.Initialize("Atom Engine - Music Playback Example", atom::algo::Vec2{720, 720},
                                              atom::backend::RenderBackendId::SdlGpu);
    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    // Backend switches reset the MusicPlayer (see MusicDebugger::SwitchBackend),
    // so the tracks are re-loaded from the same VFS files on demand.
    const auto reload_tracks = [&music, &filesystem, &music1, &music2]() {
        return music.Load("registerId_1", *filesystem, *music1) &&
               music.Load("registerId_2", *filesystem, *music2);
    };

    MusicDebugger debugger{music, music_fade, reload_tracks};
    debugger.Attach(window);

    application.Run();
    return 0;
}

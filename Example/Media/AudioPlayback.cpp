/**
  * @file           : MusicPlayback.cpp
  * @author         : Romi Brooks
  * @brief          : Music playback demo with crossfade switching between two
  *                   tracks.
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Media/Audio/AudioBackend.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Audio/Transitions/MusicCrossfade.hpp> // fade in plugin

#include <Filesystem/FileSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

#include <Window/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Debugger/Overlay.hpp>
#include <Debugger/LogDebugger.hpp>

#include <Log/LogSystem.hpp>

namespace {
// replace it
constexpr auto MusicDir = R"(E:\Music\)";
constexpr auto Music1Name = "我的歌声里 - 曲婉婷.mp3";
constexpr auto Music2Name = "滴滴 - 覆予.mp3";

class MusicDebugger final : public atom::debugger::DebugPanel {
    public:
        MusicDebugger(atom::MusicPlayer& music, atom::audio::MusicCrossfade& fade)
            : DebugPanel("MusicDebugger"), music_(music), fade_(fade) {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("Music Debugger");

            ImGui::Text("Press A to play music1, B to play music2");
            static std::string now_key_playing;

            constexpr float fade_time = 2.0f;

            if (ImGui::Button("A")) {
                if (now_key_playing != "registerId_1") {
                    fade_.Switch("registerId_1", fade_time);
                    now_key_playing = "registerId_1";
                }
            }

            if (ImGui::Button("B")) {
                if (now_key_playing != "registerId_2") {
                    fade_.Switch("registerId_2", fade_time);
                    now_key_playing = "registerId_2";
                }
            }

            ImGui::Text("If one of them is playing, switch it to the aim song");
            ImGui::Separator();

            const auto& backend_id = atom::audio::GetAudioBackendId();
            ImGui::TextDisabled("Active audio backend: %s", backend_id.c_str());
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
        atom::MusicPlayer& music_;
        atom::audio::MusicCrossfade& fade_;
};

class MusicScreen final : public atom::Screen {
    public:
        explicit MusicScreen(atom::audio::MusicCrossfade& transition) : transition_(transition) {}

        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::render::Color{.r = 30, .g = 30, .b = 60});
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.key == atom::event::Key::Escape) {
                    atom::RenderWindow::GetInstance().Shutdown();
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

auto main() -> int {
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
    atom::fs::AssetPath music1{};
    atom::fs::AssetPath music2{};
    if (!atom::fs::AssetPath::TryParse("res://" + std::string{Music1Name}, music1) ||
        !atom::fs::AssetPath::TryParse("res://" + std::string{Music2Name}, music2)) {
        LOG_ERROR(atom::log::audio::Music, "Music filenames are not valid AssetPath segments");
        return 1;
    }
    music.Load("registerId_1", *filesystem, music1);
    music.Load("registerId_2", *filesystem, music2);

    auto* music_screen =
        atom::ScreenManager::GetInstance().LoadScreen("MusicPlayback", std::make_unique<MusicScreen>(music_fade));
    atom::ScreenManager::GetInstance().SwitchScreen(music_screen);

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - Music Playback Example", atom::algo::Vec2{720, 720},
                      atom::backend::RenderBackendId::SdlGpu);

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    MusicDebugger debugger{music, music_fade};
    debugger.Attach(window);

    window.Run();
}

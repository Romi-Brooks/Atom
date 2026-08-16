/**
  * @file           : MusicPlayback.cpp
  * @author         : Romi Brooks
  * @brief          : Music playback demo with crossfade switching between two
  *                   tracks.
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Backend/Runtime/BackendRuntime.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Audio/Transitions/MusicCrossfade.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Window/Overlay.hpp>

#include <Log/LogSystem.hpp>

#ifdef _WIN32
#include "windows.h"
#endif // _WIN32

namespace {
constexpr auto kMusic1Path = R"(E:\Music\我的歌声里 - 曲婉婷.mp3)";
constexpr auto kMusic2Path = R"(E:\Music\滴滴 - 覆予.mp3)";

class MusicDebugger final : public atom::Debugger {
    public:
        MusicDebugger(atom::MusicPlayer& music, atom::audio::MusicCrossfade& fade) : music_(music), fade_(fade) {}

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

            ImGui::TextDisabled("Playback Backend: SDL3 (only registered playback backend)");
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

        auto Render(atom::IRenderTarget& target) -> void override {
            target.Clear(atom::Color{30, 30, 60});
        }

        auto HandleEvent(const atom::IEvent& event) -> bool override {
            if (event.type == atom::EventType::KeyPressed) {
                const auto& key = std::get<atom::KeyEvent>(event.data);
                if (key.scancode == 41) { // SDL_SCANCODE_ESCAPE
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
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif // _WIN32

    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};
    atom::audio::MusicCrossfade music_fade{music};

    music.Load("registerId_1", kMusic1Path);
    music.Load("registerId_2", kMusic2Path);

    atom::ScreenManager::GetInstance().LoadScreen("Music", std::make_unique<MusicScreen>(music_fade));
    atom::ScreenManager::GetInstance().SwitchScreen("Music");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - Music Playback Example", atom::Vec2{720, 720});

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    MusicDebugger debugger{music, music_fade};
    debugger.Attach(window);

    window.Run();
}

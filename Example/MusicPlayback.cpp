/**
  * @file           : MusicPlayback.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <imgui.h>

#include <Media/Audio/Music/Music.hpp>
#include <Media/Audio/Plugs/MusicFade.hpp>
#include <Window/Screen.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Debugger.hpp>
#include <Log/LogSystem.hpp>

#ifdef _WIN32
#include "windows.h"
#endif // _WIN32

class MusicDebugger final : public atom::Debugger {
    public:
        MusicDebugger(atom::Music& music, atom::audio::MusicFade& fade)
            : music_(music), fade_(fade) {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("Music Debugger");

            ImGui::Text("Press A to play music1, B to play music2");
            static std::string now_key_playing;

            constexpr float fade_time = 2.0f;

            if (ImGui::Button("A")) {
                if (now_key_playing != "registerId_1")
                {
                    fade_.Switch("registerId_1", fade_time);
                    now_key_playing = "registerId_1";
                }
            }

            if (ImGui::Button("B")) {
                if (now_key_playing != "registerId_2")
                {
                    fade_.Switch("registerId_2", fade_time);
                    now_key_playing = "registerId_2";
                }
            }

            ImGui::Text("If one of them is playing, switch it to the aim song");
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
        atom::Music& music_;
        atom::audio::MusicFade& fade_;
    };


class MusicScreen final : public atom::Screen {
    public:
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

        auto Update(float) -> void override {
        }
};

auto main() -> int {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif // _WIN32

    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    atom::Music music;
    atom::audio::MusicFade music_fade{music};

    music.Load("registerId_1", R"(E:\Music\永恒 - 幼稚园杀手.wav)");
    music.Load("registerId_2", R"(E:\Music\1_So Far Away (feat. Jamie Scott & Romy Dya)_(Instrumental).wav)");

    atom::ScreenManager::GetInstance().LoadScreen("Music", std::make_unique<MusicScreen>());
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

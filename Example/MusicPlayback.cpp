/**
  * @file           : MusicPlayback.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// Third Party Library
#include <SFML/Graphics.hpp>
#include <imgui.h>

// Engine Headers
#include <Media/Audio/Music.hpp>
#include "Media/Audio/Plug/MusicFade.hpp"

#include <Window/Screen.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Debugger.hpp>

using Screen = atom::Screen;
using Debugger = atom::Debugger;
using Music = atom::Music;
using Fade = atom::audio::MusicFade;

class SFXDebugger final : public Debugger {
    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("SFX Debugger");

            ImGui::Text("Press A to play music1, B to play music2");
            static std::string now_key_playing;

            constexpr float fade_time = 2.0f;

            if (ImGui::Button("A")) {
                if (now_key_playing != "registerId_1")
                {
                    Fade::GetInstance().Switch("registerId_1", fade_time);
                    now_key_playing = "registerId_1";
                }
            }

            if (ImGui::Button("B")) {
                if (now_key_playing != "registerId_2")
                {
                    Fade::GetInstance().Switch("registerId_2", fade_time);
                    now_key_playing = "registerId_2";
                }
            }

            ImGui::Text("If one of them is playing, switch it to the aim song");
            ImGui::Separator();

            ImGui::Text("this btm allows you play those file at the same time");
            if (ImGui::Button("Play")) {
                // if playing
                Music::GetInstance().Stop("registerId_1");
                Music::GetInstance().Stop("registerId_2");

                Music::GetInstance().Play("registerId_1");
                Music::GetInstance().Play("registerId_2");
            }
            ImGui::Separator();

            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }
};


class MusicScreen final : public Screen {
    public:
        auto Render(sf::RenderWindow& window) -> void override {
            // dark blue background
            window.clear(sf::Color(30, 30, 60));
        }

        auto HandleEvent(const sf::Event& event) -> bool override {
            // Close the window when Escape is pressed
            if (event.is<sf::Event::KeyPressed>()) {
                const auto& key = event.getIf<sf::Event::KeyPressed>();
                if (key->code == sf::Keyboard::Key::Escape) {
                    // Access the RenderWindow singleton and close the window
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
    Music::GetInstance().Load("registerId_1",R"(E:\Music\永恒 - 幼稚园杀手.wav)");
    Music::GetInstance().Load("registerId_2",R"(E:\Music\1_So Far Away (feat. Jamie Scott & Romy Dya)_(Instrumental).wav)");

    auto& screenManager = atom::ScreenManager::GetInstance();
    screenManager.LoadScreen("Music", std::make_unique<MusicScreen>());
    screenManager.SwitchScreen("Music");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine -Music Playback Example", atom::Vec2{720, 720});

    SFXDebugger debugger {};
    debugger.Attach(window);

    window.Run();
}
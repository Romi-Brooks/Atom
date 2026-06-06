/**
  * @file           : SFXPlayback.cpp
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
#include <Media/Audio/SFX.hpp>

#include <Window/Screen.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Debugger.hpp>

using Screen = atom::Screen;
using Debugger = atom::Debugger;
using SFX = atom::SFX;

class SFXDebugger final : public Debugger {
    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("SFX Debugger");
            ImGui::Text("Press A to play sfx1, B to play sfx2");
            if (ImGui::Button("A")) {
                SFX::GetInstance().Play("registerId_1");
            }
            if (ImGui::Button("B")) {
                SFX::GetInstance().Play("registerId_2");
            }
            ImGui::Separator();

            ImGui::Text("this btm allows you play those file at the same time:");
            if (ImGui::Button("Play")) {
                SFX::GetInstance().Play("registerId_1");
                SFX::GetInstance().Play("registerId_2");
            }
            ImGui::Separator();

            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }
};


class SFXScreen final : public Screen {
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
    SFX::GetInstance().Load("registerId_1", R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\Cymatics - Vocal Essentials One Shot 1 - C.wav)");
    SFX::GetInstance().Load("registerId_2", R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\Cymatics - Vocal Essentials One Shot 2 - C.wav)");

    auto& screenManager = atom::ScreenManager::GetInstance();
    screenManager.LoadScreen("SFX", std::make_unique<SFXScreen>());
    screenManager.SwitchScreen("SFX");


    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine -SFX Playback Example", atom::Vec2{720, 720});

    SFXDebugger debugger {};
    debugger.Attach(window);

    window.Run();
}
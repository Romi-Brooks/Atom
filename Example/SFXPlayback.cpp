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
#include <Media/Audio/SFX/SFX.hpp>
#include <Window/Screen.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Debugger.hpp>

class SFXDebugger final : public atom::Debugger {
    public:
        explicit SFXDebugger(atom::SFX& sfx) : sfx_(sfx) {}

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

            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }

    private:
        atom::SFX& sfx_;
};


class SFXScreen final : public atom::Screen {
public:
    auto Render(sf::RenderWindow& window) -> void override {
        window.clear(sf::Color(30, 30, 60));
    }

    auto HandleEvent(const sf::Event& event) -> bool override {
        if (event.is<sf::Event::KeyPressed>()) {
            const auto& key = event.getIf<sf::Event::KeyPressed>();
            if (key->code == sf::Keyboard::Key::Escape) {
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
    atom::SFX sfx;

    sfx.Load("registerId_1", R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\Cymatics - Vocal Essentials One Shot 1 - C.wav)");
    sfx.Load("registerId_2", R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\Cymatics - Vocal Essentials One Shot 2 - C.wav)");

    atom::ScreenManager::GetInstance().LoadScreen("SFX", std::make_unique<SFXScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("SFX");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - SFX Playback Example", atom::Vec2{720, 720});

    SFXDebugger debugger{sfx};
    debugger.Attach(window);

    window.Run();
}

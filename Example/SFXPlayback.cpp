/**
  * @file           : SFXPlayback.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <imgui.h>

#include <Media/Audio/SFX/SFX.hpp>
#include <Window/Screen.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Debugger.hpp>
#include <Log/LogSystem.hpp>

#ifdef _WIN32
#include "windows.h"
#endif // _WIN32


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
    atom::SFX sfx;

    sfx.Load("registerId_1", R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\Cymatics - Vocal Essentials One Shot 1 - C.wav)");
    sfx.Load("registerId_2", R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\Cymatics - Vocal Essentials One Shot 2 - C.wav)");

    atom::ScreenManager::GetInstance().LoadScreen("SFX", std::make_unique<SFXScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("SFX");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - SFX Playback Example", atom::Vec2{720, 720});

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    SFXDebugger debugger{sfx};
    debugger.Attach(window);

    window.Run();
}

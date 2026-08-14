/**
  * @file           : SimpleWindow.cpp
  * @author         : Romi Brooks
  * @brief          : Simple single-window rendering example using Atom Engine API
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Window/RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Screen.hpp>
#include <Log/LogSystem.hpp>

#ifdef _WIN32
#include "windows.h"
#endif // _WIN32

class ExampleScreen final : public atom::Screen {
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

    auto Update(float) -> void override {}
};

auto main() -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32

    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    atom::ScreenManager::GetInstance().LoadScreen("Example", std::make_unique<ExampleScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("Example");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - Simple Window Example", atom::Vec2{1280, 720});

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    window.Run();

    return 0;
}

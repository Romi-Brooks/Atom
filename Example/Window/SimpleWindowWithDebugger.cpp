/**
  * @file           : SimpleWindowWithDebugger.cpp
  * @author         : Romi Brooks
  * @brief          : Single-window example with custom debug overlay.
  * @attention      : Demonstrates how to inherit atom::Debugger for custom content
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Window/Manager/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Window/Overlay.hpp>

#include <Log/LogSystem.hpp>

#ifdef _WIN32
#include "windows.h"
#endif // _WIN32

namespace {
class ExampleDebugger final : public atom::Debugger {
    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("Example Debugger");
            ImGui::Text("FPS: %.1f", GetFPS());
            ImGui::Separator();
            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }
};

class ExampleScreen final : public atom::Screen {
    public:
        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::render::Color{30, 30, 60});
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.scancode == 41) { // SDL_SCANCODE_ESCAPE
                    atom::RenderWindow::GetInstance().Shutdown();
                    return true;
                }
            }
            return false;
        }

        auto Update(float) -> void override {}
};
} // namespace

auto main() -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32

    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    atom::ScreenManager::GetInstance().LoadScreen("Example", std::make_unique<ExampleScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("Example");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - Debug Overlay Example", atom::algo::Vec2{1280, 720});

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    ExampleDebugger debugger{};
    debugger.Attach(window);
    debugger.SetLoggerEnabled(true);

    window.Run();
}

/**
  * @file           : SimpleWindow.cpp
  * @author         : Romi Brooks
  * @brief          : Simple single-window rendering example using Atom Engine
  *                   API.
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Window/Manager/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>

#include <Log/LogSystem.hpp>

namespace {
class ExampleScreen final : public atom::Screen {
    public:
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

        auto Update(float) -> void override {}
};
} // namespace

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    atom::ScreenManager::GetInstance().LoadScreen("Example", std::make_unique<ExampleScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("Example");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - Simple Window Example", atom::algo::Vec2{1280, 720});

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    window.Run();

    return 0;
}

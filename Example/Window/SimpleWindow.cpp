/**
  * @file           : SimpleWindow.cpp
  * @author         : Romi Brooks
  * @brief          : Simple single-window rendering example using Atom Engine
  *                   API.
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Window/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>

#include <Log/LogSystem.hpp>

namespace {
// Inheriting from the abstract Screen class
// Override methods to determine the page/content/behavior
class ExampleScreen final : public atom::Screen {
    public:
        // Render content
        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::color::Color::Black());
        }

        // Event Handler
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
    // For windows: Console Debugger OUTPUT CP will be set to UTF-8
    atom::Log::SetConsoleOutputUtf8();

    // Log level
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    // Screen Register (Best practice)
    const auto exampleScreen = atom::ScreenManager::GetInstance().LoadScreen("Example", std::make_unique<ExampleScreen>());
    // Select this screen
    atom::ScreenManager::GetInstance().SwitchScreen(exampleScreen);
    //
    // // Usually, explicitly confirming that a screen has no post-tasks, you can construct it in place:
    //  atom::ScreenManager::GetInstance().LoadScreen("Example", std::make_unique<ExampleScreen>());
    // // You can also switch to the target screen using a string.
    //  atom::ScreenManager::GetInstance().SwitchScreen("Example");

    // Give a window instance
    auto& window = atom::RenderWindow::GetInstance();
    // Init the window instance, Can pass a render backend explicit
    window.Initialize("Atom Engine - Simple Window Example",  // Title
                      atom::algo::Vec2{1280, 720},  // Resolution
                      atom::backend::RenderBackendId::SdlGpu  // Render Backend
                      );

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    // Main loop
    window.Run();
}

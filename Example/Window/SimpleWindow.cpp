/**
  * @file           : SimpleWindow.cpp
  * @author         : Romi Brooks
  * @brief          : Simple single-window rendering example using Atom Engine
  *                   API.
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Atom/Platform/ApplicationEntry.hpp>
#include <Atom/Runtime/ScreenManager.hpp>
#include <Atom/Platform/Window/RenderWindow.hpp>
#include <Atom/Runtime/Application.hpp>
#include <Atom/Runtime/Screen.hpp>

#include <Atom/Core/Log/LogSystem.hpp>

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
        //
        // INPUT: two paths exist and they are not interchangeable.
        //
        //   This one (event style) - a discrete command that must fire once per
        //   press and wants the exact event payload, here "Escape went down".
        //   Returning true consumes the event.
        //
        //   input::InputSystem (polling style) - anything continuous: "is this
        //   key held", "how far did the mouse move", "was a chord pressed this
        //   frame", and the only way to ask the same question from more than one
        //   place in a frame. Add <Input/InputSystem.hpp> and read it from
        //   Update()/FixedUpdate().
        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.key == atom::input::Key::Escape) {
                    atom::RenderWindow::GetInstance().RequestClose();
                    return true;
                }
            }
            return false;
        }

        auto Update(float) -> void override {}
};
} // namespace

auto atom::platform::ApplicationMain(int, char**) -> int {
    // For windows: Console Debugger OUTPUT CP will be set to UTF-8
    atom::Log::SetConsoleOutputUtf8();

    // Log level
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    // Screen Register (Best practice). LoadScreen/UnloadScreen report a
    // ScreenOpResult so a rejected duplicate registration cannot go unnoticed
    // (ARCH-105); pass ScreenReplacePolicy::Replace to re-register a name.
    const auto loaded =
        atom::ScreenManager::GetInstance().LoadScreen("Example", std::make_unique<ExampleScreen>());
    if (loaded != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager,
                  "Failed to register the example screen: " + std::string{atom::ToString(loaded)});
        return 1;
    }
    // Select this screen by name; the manager owns it. Read the pointer back
    // with GetScreen() when a raw Screen* is needed (never capture one before
    // LoadScreen: a rejected registration destroys the screen).
    atom::ScreenManager::GetInstance().SwitchScreen("Example");

    // The application owns the window and the frame loop.
    auto& application = atom::runtime::Application::GetInstance();
    // Init the window instance, Can pass a render backend explicit. Loading a
    // render backend can fail; the reason comes back instead of only reaching
    // the log, so a headless CI run fails on the actual cause.
    const auto initialized = application.Initialize("Atom Engine - Simple Window Example",  // Title
                                              atom::algo::Vec2{1280, 720},  // Resolution
                                              atom::backend::RenderBackendId::SdlGpu  // Render Backend
    );
    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }

    // The startup frame cap is part of the config (ApplicationConfig::fps,
    // default 60; 0 = uncapped), so this convenience overload needs no explicit
    // SetFPS call. window.SetFPS() remains available for runtime changes.

    // Main loop
    application.Run();
    return 0;
}

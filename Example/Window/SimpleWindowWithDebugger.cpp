/**
  * @file           : SimpleWindowWithDebugger.cpp
  * @author         : Romi Brooks
  * @brief          : Single-window example with custom debug overlay.
  * @attention      : Window-scoped LogDebugger + screen-scoped custom panel.
  *                   Toggles the log panel via LogDebugger::Get() (no injection).
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Window/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Debugger/Overlay.hpp>
#include <Debugger/LogDebugger.hpp>

#include <Log/LogSystem.hpp>

namespace {

class ExamplePanel final : public atom::debugger::DebugPanel {
    private:
        bool slot_applied_ = false;

    protected:
        [[nodiscard]] auto GetPanelName() const -> const char* override {
            return "ExamplePanel";
        }

        auto OnDrawOverlay() -> void override {
            if (!slot_applied_) {
                atom::debugger::ApplyStatusPanelSlot();
                slot_applied_ = true;
            }
            ImGui::Begin("Example Debugger");
            ImGui::Text("FPS: %.1f", GetFPS());
            ImGui::Separator();
            ImGui::Text("Press ESC to exit");
            // Well-known log panel — no constructor injection required.
            if (auto* log_panel = atom::debugger::LogDebugger::Get()) {
                if (ImGui::Button(log_panel->IsEnabled() ? "Hide Log Debugger" : "Show Log Debugger")) {
                    log_panel->SetEnabled(!log_panel->IsEnabled());
                }
            }
            ImGui::End();
        }
};

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

    // Screen Register
    auto* example_screen = atom::ScreenManager::GetInstance().LoadScreen(
        "Example", std::make_unique<ExampleScreen>());

    // Select this screen
    atom::ScreenManager::GetInstance().SwitchScreen(example_screen);

    // Give a window instance
    auto& window = atom::RenderWindow::GetInstance();
    // Init the window instance, Can pass a render backend explicit
    window.Initialize("Atom Engine - Debug Overlay Example",  // Title (also the window name)
                      atom::algo::Vec2{1280, 720},  // Resolution
                      atom::backend::RenderBackendId::SdlGpu  // Render Backend
                      );

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    // Attach the custom debug panel to this screen scoped (only while Example is visible)
    ExamplePanel example_panel{};
    example_panel.Attach(window, *example_screen);

    // Attach the Built-in Log Debugger to the window scoped (globally)
    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    // Main loop
    window.Run();
}

/**
  * @file           : SimpleWindowWithDebugger.cpp
  * @author         : Romi Brooks
  * @brief          : Single-window example with custom debug overlay.
  * @attention      : Demonstrates DebugPanel inheritance and standalone LogDebugger
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Backend/Contracts/Render/RenderBackendId.hpp>
#include <Event/Input.hpp>
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
        atom::debugger::LogDebugger* log_panel_ = nullptr;

    public:
        explicit ExamplePanel(atom::debugger::LogDebugger* log_panel) : log_panel_(log_panel) {}

    protected:
        auto OnDrawOverlay() -> void override {
            if (!slot_applied_) {
                atom::debugger::ApplyStatusPanelSlot();
                slot_applied_ = true;
            }
            ImGui::Begin("Example Debugger");
            ImGui::Text("FPS: %.1f", GetFPS());
            ImGui::Separator();
            ImGui::Text("Press ESC to exit");
            if (log_panel_ != nullptr && ImGui::Button(log_panel_->IsEnabled() ? "Hide Log Debugger" : "Show Log Debugger")) {
                log_panel_->SetEnabled(!log_panel_->IsEnabled());
            }
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
    window.Initialize("Atom Engine - Debug Overlay Example", atom::algo::Vec2{1280, 720},
                      atom::backend::RenderBackendId::SdlGpu);
    window.SetFPS(60);

    // Panels are peers: attach the built-in log viewer and any custom DebugPanel.
    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    ExamplePanel stats_panel{&log_panel};
    stats_panel.Attach(window);

    window.Run();
}

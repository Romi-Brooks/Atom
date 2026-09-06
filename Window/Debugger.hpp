/**
  * @file           : Debugger.hpp
  * @brief          : Abstract base class for shared ImGui debug panels.
  */

#ifndef ATOM_DEBUGGER_HPP
#define ATOM_DEBUGGER_HPP

#include <cstddef>
#include <memory>

namespace atom::debugger {
class OverlayConnection;
}

namespace atom {

class RenderWindow;
class ListenerConnection;
class LogDebugger;

// Registers one panel with the RenderWindow-owned shared ImGui context.
// Multiple debuggers/overlays can coexist and Detach() only removes this
// debugger's own panel and update listener.
class Debugger {
    private:
        bool attached_ = false;
        RenderWindow* target_window_ = nullptr;
        std::unique_ptr<atom::debugger::OverlayConnection> overlay_connection_;
        std::unique_ptr<ListenerConnection> update_connection_;
        std::unique_ptr<LogDebugger> log_debugger_;
        bool enabled_ = true;
        bool logger_enabled_ = false;

        // FPS tracking
        std::size_t frame_count_ = 0;
        float fps_accumulator_ = 0.0f;
        float fps_display_ = 0.0f;

    public:
        Debugger() = default;
        virtual ~Debugger();

        Debugger(const Debugger&) = delete;
        auto operator=(const Debugger&) -> Debugger& = delete;

        // Attach to a RenderWindow and register one panel in its shared ImGui
        // context.
        auto Attach(RenderWindow& window) -> void;

        // Detach only this panel; the shared ImGui backend remains alive while
        // other panels are attached.
        auto Detach() -> void;

        [[nodiscard]] auto IsAttached() const -> bool {
            return attached_;
        }

        virtual auto SetEnabled(bool enabled) -> void {
            enabled_ = enabled;
        }

        [[nodiscard]] auto IsEnabled() const -> bool {
            return enabled_;
        }

        // Enables the built-in LogDebugger component. The component is
        // attached to the same shared ImGui context as this Debugger.
        auto SetLoggerEnabled(bool enabled) -> void;

        [[nodiscard]] auto IsLoggerEnabled() const -> bool {
            return logger_enabled_;
        }

        [[nodiscard]] auto GetFPS() const -> float {
            return fps_display_;
        }

    protected:
        // Override this to draw the panel. Called inside the shared ImGui
        // frame; use ImGui::Begin/End here.
        virtual auto OnDrawOverlay() -> void {}
};

} // namespace atom

#endif // ATOM_DEBUGGER_HPP

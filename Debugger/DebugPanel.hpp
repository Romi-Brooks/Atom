// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file DebugPanel.hpp
 * @brief Abstract base for every panel registered on the shared ImGui overlay.
 */

#ifndef ATOM_DEBUGGER_DEBUG_PANEL_HPP
#define ATOM_DEBUGGER_DEBUG_PANEL_HPP

#include <cstddef>
#include <memory>
#include <string>

#include <Debugger/DebuggerConfig.hpp>

namespace atom {
class RenderWindow;
class Screen;
class ListenerConnection;
}

namespace atom::debugger {

class OverlayConnection;

// One panel in the RenderWindow-owned shared ImGui context. Subclasses
// implement OnDrawOverlay(); multiple panels coexist independently.
//
// Lifetime scopes:
//   Attach(window)          — window-level, visible for the whole run
//                             (e.g. LogDebugger).
//   Attach(window, screen)  — screen-level, drawn only while that Screen is
//                             visible (current or on the screen stack).
class DebugPanel {
    private:
        bool attached_ = false;
        bool enabled_ = true;
        atom::RenderWindow* target_window_ = nullptr;
        atom::Screen* bound_screen_ = nullptr;
        // Cached at Attach while the derived object is still alive — virtual
        // GetPanelName() is not safe from ~DebugPanel (derived already gone).
        std::string panel_name_{"DebugPanel"};
        // "window-scoped" | "screen-scoped", snapshotted with panel_name_.
        std::string panel_scope_{"window-scoped"};
        std::unique_ptr<OverlayConnection> overlay_connection_;
        std::unique_ptr<atom::ListenerConnection> update_connection_;

        std::size_t frame_count_ = 0;
        float fps_accumulator_ = 0.0f;
        float fps_display_ = 0.0f;

    public:
        DebugPanel() = default;
        virtual ~DebugPanel();

        DebugPanel(const DebugPanel&) = delete;
        auto operator=(const DebugPanel&) -> DebugPanel& = delete;

        // Window-level: survives screen switches.
        auto Attach(atom::RenderWindow& window) -> void;
        // Screen-level: follows that Screen's visibility (stack + current).
        auto Attach(atom::RenderWindow& window, atom::Screen& screen) -> void;
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

        [[nodiscard]] auto GetFPS() const -> float {
            return fps_display_;
        }

        [[nodiscard]] auto GetBoundScreen() const -> atom::Screen* {
            return bound_screen_;
        }

    protected:
        // Called inside the shared ImGui frame when the panel is enabled and
        // its bound screen (if any) is visible.
        virtual auto OnDrawOverlay() -> void {}

        // Lifetime hooks for panel-specific subscriptions (e.g. LogDebugger).
        // Return false to reject Attach (e.g. a well-known slot is taken).
        //
        // Subclasses that override OnDetach MUST call Detach() in their own
        // destructor first — by ~DebugPanel the derived part is already gone,
        // so virtual dispatch would hit the base no-op.
        virtual auto OnAttach(atom::RenderWindow& window) -> bool;
        virtual auto OnDetach() -> void;

        [[nodiscard]] virtual auto GetPanelName() const -> const char* {
            return "DebugPanel";
        }

        [[nodiscard]] auto GetTargetWindow() const -> atom::RenderWindow* {
            return target_window_;
        }

    private:
        auto AttachScoped(atom::RenderWindow& window, atom::Screen* screen) -> void;
        [[nodiscard]] auto IsScopeVisible() const -> bool;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_DEBUG_PANEL_HPP

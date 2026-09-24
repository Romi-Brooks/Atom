// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file DebugPanel.hpp
 * @brief Abstract base for every panel registered on the shared ImGui overlay.
 */

#ifndef ATOM_DEBUGGER_DEBUG_PANEL_HPP
#define ATOM_DEBUGGER_DEBUG_PANEL_HPP

#include <memory>
#include <string>

#include <Debugger/DebuggerConfig.hpp>

namespace atom {
class RenderWindow;
class Screen;
class ListenerConnection;
}

enum class PanelScope {
    Window,
    Screen,
};

namespace atom::debugger {

class OverlayConnection;

// One panel in the RenderWindow-owned shared ImGui context.
// Subclasses implement OnDrawOverlay();
// multiple panels coexist independently.
//
//   Attach(window)          — window-level, visible for the whole run, global Debugger
//   Attach(window, screen)  — screen-level, drawn only while that Screen is visible

class DebugPanel {
    private:
        bool attached_ = false;
        bool enabled_ = true;

        atom::RenderWindow* target_window_ = nullptr;
        atom::Screen* bound_screen_ = nullptr;

        std::string panel_name_ = {};

        PanelScope panel_scope_ = {};

        std::unique_ptr<OverlayConnection> overlay_connection_;

    public:
        explicit DebugPanel(std::string panel_name = "DebugPanel");
        virtual ~DebugPanel();

        DebugPanel(const DebugPanel&) = delete;
        auto operator=(const DebugPanel&) -> DebugPanel& = delete;

        // Window-level
        auto Attach(atom::RenderWindow& window) -> void;
        // Screen-level
        auto Attach(atom::RenderWindow& window, atom::Screen& screen) -> void;

        auto Detach() -> void;

        [[nodiscard]] auto IsAttached() const -> bool {
            return attached_;
        }

        virtual auto SetEnabled(const bool enabled) -> void {
            enabled_ = enabled;
        }

        [[nodiscard]] auto IsEnabled() const -> bool {
            return enabled_;
        }

        [[nodiscard]] auto GetPanelName() const -> const std::string& {
            return panel_name_;
        }
        [[nodiscard]] auto GetPanelScope() const -> PanelScope {
            return panel_scope_;
        }

        [[nodiscard]] auto GetBoundScreen() const -> atom::Screen* {
            return bound_screen_;
        }

        [[nodiscard]] auto GetPanelScopeName() const -> const char*;

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

        [[nodiscard]] auto GetTargetWindow() const -> atom::RenderWindow* {
            return target_window_;
        }

    private:
        auto AttachScoped(atom::RenderWindow& window, atom::Screen* screen) -> void;
        [[nodiscard]] auto IsScopeVisible() const -> bool;
        auto RollbackAttach() -> void;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_DEBUG_PANEL_HPP

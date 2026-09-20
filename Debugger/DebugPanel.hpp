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

namespace atom {
class RenderWindow;
class ListenerConnection;
}

namespace atom::debugger {

class OverlayConnection;

// One panel in the RenderWindow-owned shared ImGui context. Subclasses
// implement OnDrawOverlay(); multiple panels coexist independently.
class DebugPanel {
    private:
        bool attached_ = false;
        bool enabled_ = true;
        atom::RenderWindow* target_window_ = nullptr;
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

        auto Attach(atom::RenderWindow& window) -> void;
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

    protected:
        // Called inside the shared ImGui frame when the panel is enabled.
        virtual auto OnDrawOverlay() -> void {}

        // Lifetime hooks for panel-specific subscriptions (e.g. LogDebugger).
        virtual auto OnAttach(atom::RenderWindow& window) -> void;
        virtual auto OnDetach() -> void;

        [[nodiscard]] auto GetTargetWindow() const -> atom::RenderWindow* {
            return target_window_;
        }
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_DEBUG_PANEL_HPP

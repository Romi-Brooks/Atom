// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file DisplayDebugger.hpp
 * @brief Panel for the two player-facing display choices.
 *
 * A thin view over atom::display::DisplaySystem: it only queues requests and
 * renders what the backend reports. All the state and the frame-boundary
 * application live in the module, so an in-game settings page can drive exactly
 * the same two values without this panel existing.
 *
 * It shows the *applied* mode next to the request, and whether the applied mode
 * really caps the frame rate on this backend: a driver or compositor policy can
 * override a present mode, and "we asked for it" is not "it is in effect".
 */

#ifndef ATOM_DEBUGGER_DISPLAY_DEBUGGER_HPP
#define ATOM_DEBUGGER_DISPLAY_DEBUGGER_HPP

#include <Debugger/DebugPanel.hpp>

namespace atom::debugger {

class DisplayDebugger final : public DebugPanel {
    public:
        DisplayDebugger() : DebugPanel("DisplayDebugger") {}

    protected:
        auto OnDrawOverlay() -> void override;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_DISPLAY_DEBUGGER_HPP

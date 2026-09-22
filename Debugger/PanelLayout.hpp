// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file PanelLayout.hpp
 * @brief Deterministic first-open placement for debug panels (no imgui.ini).
 */

#ifndef ATOM_DEBUGGER_PANEL_LAYOUT_HPP
#define ATOM_DEBUGGER_PANEL_LAYOUT_HPP

namespace atom::debugger {

auto ApplyPanelSlot(float pos_x, float pos_y, float size_w, float size_h) -> void;
auto ApplyLogPanelSlot() -> void;
auto ApplyStatusPanelSlot() -> void;

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_PANEL_LAYOUT_HPP

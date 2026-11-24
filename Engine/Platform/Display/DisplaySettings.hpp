// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file DisplaySettings.hpp
 * @brief The two player-facing display choices, as one value.
 *
 * Window placement and present mode are set together by the same settings page,
 * restored together from the same save entry, and are only meaningful as a pair:
 * "Immediate" says nothing without knowing whether the window is fullscreen,
 * because the same mode paces differently in the two placements.
 */

#ifndef ATOM_DISPLAY_DISPLAYSETTINGS_HPP
#define ATOM_DISPLAY_DISPLAYSETTINGS_HPP

#include <string>

#include <Backend/Contracts/Render/PresentMode.hpp>
#include <Backend/Contracts/Window/WindowMode.hpp>

namespace atom::display {

// One display configuration. Defaults are the conservative desktop case a
// first launch should use: a windowed, vsync-paced window.
//
// Deliberately does NOT carry an fps limit: that belongs to the frame pacer
// (IWindow::SetFPS / RenderWindow::SetFPS), which every program drives on its
// own schedule, and folding it in here would give the same knob two owners.
struct DisplaySettings {
        window::WindowMode window_mode = window::WindowMode::Windowed;
        render::PresentMode present_mode = render::PresentMode::VSync;

        bool operator==(const DisplaySettings&) const = default;
};

// "borderless+immediate" style, for logs and memos: one line that identifies the
// exact pair a measurement was taken with.
[[nodiscard]] inline auto ToString(const DisplaySettings& settings) -> std::string {
    return std::string{window::ToString(settings.window_mode)} + "+" +
           std::string{render::ToString(settings.present_mode)};
}

} // namespace atom::display

#endif // ATOM_DISPLAY_DISPLAYSETTINGS_HPP

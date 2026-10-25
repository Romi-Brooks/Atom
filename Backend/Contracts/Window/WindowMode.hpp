/**
  * @file           : WindowMode.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral window placement modes.
  * @attention      : Orthogonal to atom::render::PresentMode, but the two
  *                   interact on every desktop platform (see below).
  * @date           : 2026/10/23
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_WINDOW_WINDOWMODE_HPP
#define ATOM_BACKEND_CONTRACTS_WINDOW_WINDOWMODE_HPP

#include <cstdint>
#include <string_view>

namespace atom::window {

// How the host window occupies the display. This is a *placement* answer and
// says nothing about how frames reach the panel; that is
// atom::render::PresentMode. Keep them separate, because the two are set by
// different user-facing options ("display mode" vs "vertical sync") and because
// one placement can sit on several present modes.
//
// There are exactly three placements, the same trio every game's display page
// offers. Note that there is deliberately no separate "borderless fullscreen"
// value: a borderless window whose configured size equals the display size *is*
// the borderless-fullscreen experience, and giving it its own mode would force
// callers to choose between two spellings of one state.
//
//   Windowed
//       Decorated, resizable window at the configured size.
//
//   Borderless
//       The same window with no decorations: a borderless window at the
//       configured size. Set the resolution to the display's own size and it
//       covers the screen without a display-mode switch -- which is what
//       players call "borderless fullscreen".
//
//   ExclusiveFullscreen
//       A real display-mode switch: the swapchain owns the panel.
//
// The interaction that matters in practice, measured on Windows/D3D12: the
// desktop compositor does NOT throttle the submitter. In a window it drops the
// surplus frames instead, so a non-vsync present mode runs just as fast in a
// borderless window as in exclusive fullscreen (measured ~2500 fps at 2560x1440
// either way), and what changes is only whether the result can tear. Fullscreen
// is still the placement where a non-vsync mode *without* a tear flag gets
// forced back to vblank sync, which is why Mailbox caps there and not in a
// window. Do not predict any of this from the placement: ask
// IRenderDevice::IsPresentRefreshPaced().
enum class WindowMode : std::uint8_t {
    Windowed = 0,
    // Undecorated window at the configured size. Not a fullscreen state: it
    // stays a movable/resizable top-level window, so the compositor's
    // direct-scanout path (Independent Flip / DirectFlip on Windows,
    // unredirect on X11) is only taken when it happens to cover the display.
    Borderless,
    // Display-mode change (resolution/refresh switch). On SDL_GPU the D3D12
    // swapchain stays a windowed HWND swapchain, so whether this truly bypasses
    // the compositor is the platform's decision; the Vulkan path presents on a
    // real fullscreen surface, where PresentMode::Immediate can tear.
    ExclusiveFullscreen,
};

[[nodiscard]] constexpr auto ToString(const WindowMode mode) -> std::string_view {
    switch (mode) {
    case WindowMode::Windowed:
        return "windowed";
    case WindowMode::Borderless:
        return "borderless";
    case WindowMode::ExclusiveFullscreen:
        return "exclusive";
    }
    return "windowed";
}

// True only for the real fullscreen state. Borderless is *not* fullscreen: it
// just happens to cover the display when sized that way, and callers that need
// to reconcile a display-mode change with the backbuffer size must not treat it
// as one.
[[nodiscard]] constexpr auto IsFullscreen(const WindowMode mode) -> bool {
    return mode == WindowMode::ExclusiveFullscreen;
}

// True when the window has no title bar / decorations.
[[nodiscard]] constexpr auto IsUndecorated(const WindowMode mode) -> bool {
    return mode != WindowMode::Windowed;
}

} // namespace atom::window

#endif // ATOM_BACKEND_CONTRACTS_WINDOW_WINDOWMODE_HPP

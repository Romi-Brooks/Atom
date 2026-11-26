/**
  * @file           : PresentMode.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral swapchain present modes and their bitmask.
  * @attention      : The three modes are the ones SDL_GPU exposes; a native
  *                   backend maps them onto its own present modes.
  * @date           : 2026/10/23
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_PRESENTMODE_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_PRESENTMODE_HPP

#include <cstdint>
#include <string_view>

namespace atom::render {

// How a submitted frame reaches the display. Semantics, not implementation:
//
//   VSync      The submit stalls until the display is ready for the frame. The
//              frame rate is capped at the refresh rate, the CPU is blocked for
//              the remainder of the frame, and there is no tearing.
//   Mailbox    The submit does not stall: the frame replaces the one waiting to
//              be displayed and extra frames are dropped ("triple buffering
//              without the latency"). Whether it also *caps* the frame rate is
//              backend-dependent rather than nominal: a faithful driver mailbox
//              (VK_PRESENT_MODE_MAILBOX_KHR) waits for the vertical blank, but
//              SDL_GPU's D3D12 path emulates it with sync interval 0 and lets the
//              desktop compositor drop the surplus, which leaves the submitter
//              uncapped in a window (and vblank-synced in fullscreen, where
//              tearing is disallowed). Ask the device instead of assuming.
//   Immediate  No pacing at all: present and move on. Uncapped frame rate, at
//              the cost of tearing wherever the platform/desktop compositor
//              lets the image through unsynchronized.
//
// Read IsRefreshPaced() as the *nominal* answer only. For "is the frame rate
// actually capped right now", ask IRenderDevice::IsPresentRefreshPaced(): the
// SDL_GPU D3D12 path deviates for Mailbox, and only the device's answer matches
// what a capture tool will show.
//
// --- app request, not driver policy -----------------------------------------
// These are the modes the *application asks the swapchain for* — the value
// behind an in-game "vertical sync" option:
//
//     Off            -> Immediate
//     On             -> VSync
//     On + triple    -> Mailbox   ("fast"/"mailbox" in most engines' settings)
//
// They are deliberately not the same thing as the GPU control panel's global
// VSync setting (NVIDIA "Vertical sync", AMD "Wait for vertical refresh").
// That setting is a *driver policy* that can silently override this request:
// "Force on" turns an Immediate request into a vblank-synced present, and
// "Fast Sync" applies a driver-side mailbox-like policy of its own. The engine
// cannot read that policy back, so no backend can promise "Immediate means
// uncapped" — it can only promise the request was accepted. If a capture tool
// reports SyncInterval 1 while the app asked for Immediate, the driver or a
// compatibility shim won the argument; check the control panel before blaming
// the engine.
//
// GetPresentMode() therefore reports the mode the device *applied*, which may
// differ from the one requested, and never claims anything about driver policy.
enum class PresentMode : std::uint8_t {
    VSync = 0,
    Mailbox,
    Immediate,
};

// Number of distinct present modes; the bitmask has one bit per mode.
inline constexpr std::uint8_t present_mode_count = 3;

[[nodiscard]] constexpr auto ToString(const PresentMode mode) -> std::string_view {
    switch (mode) {
    case PresentMode::VSync:
        return "vsync";
    case PresentMode::Mailbox:
        return "mailbox";
    case PresentMode::Immediate:
        return "immediate";
    }
    return "vsync";
}

// True when the mode still displays at most one frame per refresh, as the
// vendor APIs define it: VSync and Mailbox nominally do, Immediate does not.
//
// NOMINAL ONLY. A backend may implement Mailbox differently (SDL_GPU/D3D12 runs
// it uncapped in a window), so anything that reports or branches on "is the
// frame rate capped right now" must call IRenderDevice::IsPresentRefreshPaced()
// -- this predicate cannot know the backend or the window placement.
[[nodiscard]] constexpr auto IsRefreshPaced(const PresentMode mode) -> bool {
    return mode != PresentMode::Immediate;
}

// --- capability bitmask ------------------------------------------------------
// IRenderDevice::SupportedPresentModes() reports the modes a backend accepts on
// the current window/driver, so UI can grey out the ones that would fail
// instead of probing by trial and error.

[[nodiscard]] constexpr auto ToMask(const PresentMode mode) -> std::uint8_t {
    return static_cast<std::uint8_t>(1u << static_cast<unsigned>(mode));
}

inline constexpr std::uint8_t all_present_modes =
    ToMask(PresentMode::VSync) | ToMask(PresentMode::Mailbox) | ToMask(PresentMode::Immediate);

[[nodiscard]] constexpr auto SupportsPresentMode(const std::uint8_t mask, const PresentMode mode) -> bool {
    return (mask & ToMask(mode)) != 0;
}

} // namespace atom::render

#endif // ATOM_BACKEND_CONTRACTS_RENDER_PRESENTMODE_HPP

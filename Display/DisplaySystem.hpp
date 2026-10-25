// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file DisplaySystem.hpp
 * @brief The single owner of the display policy: queue a change, apply it once
 *        per frame at the frame boundary, report what is actually applied.
 *
 * Why this is a module and not part of RenderWindow: the window facade is about
 * hosting a frame (open it, pump it, present it), while "which placement and
 * which present mode" is a *policy* that a settings page, a debug panel, a
 * command line and a config file all want to write. When the facade owned it,
 * every one of those writers needed its own deferred-change bookkeeping, and a
 * swapchain recreate could land inside a frame.
 *
 * The frame boundary is the only safe place to reconfigure a swapchain, so this
 * class holds the request until the owner of the frame loop ticks it:
 *
 *     DisplaySystem::GetInstance().Request({WindowMode::Borderless, PresentMode::Immediate});
 *     ...                                             // takes effect here
 *     DisplaySystem::GetInstance().ApplyPending(window, device);   // once per frame
 */

#ifndef ATOM_DISPLAY_DISPLAYSYSTEM_HPP
#define ATOM_DISPLAY_DISPLAYSYSTEM_HPP

#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>

#include <Display/DisplaySettings.hpp>

namespace atom::display {

class DisplaySystem final {
    public:
        [[nodiscard]] static auto GetInstance() -> DisplaySystem&;

        DisplaySystem(const DisplaySystem&) = delete;
        auto operator=(const DisplaySystem&) -> DisplaySystem& = delete;

        // Queue a request. It takes effect at the next ApplyPending(), never
        // immediately: a swapchain cannot be reconfigured while a frame is in
        // flight. Requesting the value that is already applied is a no-op.
        auto Request(const DisplaySettings& settings) -> void;
        auto RequestWindowMode(window::WindowMode mode) -> void;
        auto RequestPresentMode(render::PresentMode mode) -> void;
        // Compatibility mapping for the old two-state option: true -> VSync,
        // false -> Immediate. Prefer RequestPresentMode(): "off" hides whether
        // the caller meant Mailbox or Immediate.
        auto RequestVSync(bool enabled) -> void;

        // Apply the queued request, if any. Must be called at the frame
        // boundary -- after the previous frame was presented and before the next
        // one opens. A rejected change is dropped (the backend already logged
        // why) and `applied_` keeps the previous value, so the getters never
        // claim something that is not in effect.
        auto ApplyPending(window::IWindow& window, render::IRenderDevice& device) -> void;

        // Adopt a state without queueing anything: used at startup, where the
        // backend already came up with ApplicationConfig's values.
        auto Adopt(const DisplaySettings& settings) -> void;

        // What the backend confirmed. `window_mode`/`present_mode` are read back
        // from the backend after each successful change, so they cannot drift
        // from reality for changes this class performed. A change made behind
        // its back (the OS dropping out of fullscreen) is not tracked here --
        // ask the backend for the live value when that matters.
        [[nodiscard]] auto Applied() const -> const DisplaySettings&;
        // The applied values plus any queued change.
        [[nodiscard]] auto Requested() const -> DisplaySettings;
        [[nodiscard]] auto HasPendingChange() const -> bool {
            return pending_;
        }

    private:
        DisplaySystem() = default;

        DisplaySettings applied_{};
        DisplaySettings requested_{};
        bool pending_ = false;
};

} // namespace atom::display

#endif // ATOM_DISPLAY_DISPLAYSYSTEM_HPP

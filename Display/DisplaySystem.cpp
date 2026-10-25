// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "DisplaySystem.hpp"

#include <string>

#include <Log/LogSystem.hpp>

namespace atom::display {

auto DisplaySystem::GetInstance() -> DisplaySystem& {
    // Process-wide, like Log/TimeSystem/JobSystem: there is exactly one window
    // and one swapchain, so a second instance would only be a second opinion.
    static DisplaySystem instance;
    return instance;
}

auto DisplaySystem::Request(const DisplaySettings& settings) -> void {
    requested_ = settings;
    // Re-requesting what is already applied must not queue a swapchain
    // recreate: a settings page redraws its widgets every frame and would
    // otherwise rebuild the swapchain at frame rate.
    pending_ = requested_ != applied_;
}

auto DisplaySystem::RequestWindowMode(const window::WindowMode mode) -> void {
    auto settings = Requested();
    settings.window_mode = mode;
    Request(settings);
}

auto DisplaySystem::RequestPresentMode(const render::PresentMode mode) -> void {
    auto settings = Requested();
    settings.present_mode = mode;
    Request(settings);
}

auto DisplaySystem::RequestVSync(const bool enabled) -> void {
    RequestPresentMode(enabled ? render::PresentMode::VSync : render::PresentMode::Immediate);
}

auto DisplaySystem::Adopt(const DisplaySettings& settings) -> void {
    applied_ = settings;
    requested_ = settings;
    pending_ = false;
}

auto DisplaySystem::ApplyPending(window::IWindow& window, render::IRenderDevice& device) -> void {
    if (!pending_)
        return;
    // Clear the flag before applying: a backend that rejects one of the two
    // changes must not leave the system retrying it every frame forever.
    const DisplaySettings target = requested_;
    pending_ = false;

    // Placement first. A fullscreen transition changes the pixel size and can
    // rebuild the surface the swapchain sits on, so doing it first means the
    // present-mode call afterwards lands on the surface that stays.
    if (target.window_mode != applied_.window_mode) {
        if (window.SetWindowMode(target.window_mode)) {
            // Read back instead of trusting the request: the backend is allowed
            // to normalize (or the OS to refuse) a placement.
            applied_.window_mode = window.GetWindowMode();
        } else {
            LOG_WARNING(atom::log::core::Window,
                        std::string{"Display: window mode '"} + std::string{window::ToString(target.window_mode)} +
                            "' refused; staying in '" + std::string{window::ToString(window.GetWindowMode())} + "'");
        }
    }

    if (target.present_mode != applied_.present_mode) {
        if (device.SetPresentMode(target.present_mode)) {
            applied_.present_mode = device.GetPresentMode();
        } else {
            LOG_WARNING(atom::log::core::Window,
                        std::string{"Display: present mode '"} +
                            std::string{render::ToString(target.present_mode)} + "' refused; keeping '" +
                            std::string{render::ToString(device.GetPresentMode())} + "'");
        }
    }

    // Whatever could not be applied stays in `requested_` until the caller asks
    // for something else; `applied_` is the truth, and the panel shows it.
    requested_ = applied_;
}

auto DisplaySystem::Applied() const -> const DisplaySettings& {
    return applied_;
}

auto DisplaySystem::Requested() const -> DisplaySettings {
    return requested_;
}

} // namespace atom::display

/**
  * @file           : SDLGPUBackend.cpp
  * @author         : Romi Brooks
  * @brief          : Implements SDL_GPU render backend lifecycle orchestration.
  * @attention      : SDL_GPU native objects remain private to this backend.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDLGPUBackend.hpp"

#include <string>

#include <Log/LogSystem.hpp>

namespace atom::backend::sdlgpu {

SDLGPUBackend::~SDLGPUBackend() {
    Shutdown();
}
auto SDLGPUBackend::Initialize(const RenderBackendConfig& config) -> bool {
    // One host clock for the whole backend: the window paces frames from the
    // very ITimeSource that atom::time::TimeSystem binds to (CORE-001).
    window_.SetTimeSource(time_source_);
    if (!window_.Initialize(config.title, config.resolution))
        return false;

    // Placement before the device claims the window: a fullscreen transition
    // changes the pixel size and the surface the swapchain will be built on, so
    // applying it afterwards would mean recreating a live swapchain. A mode the
    // platform refuses is not fatal: the engine keeps running windowed.
    if (config.window_mode != window::WindowMode::Windowed && !window_.SetWindowMode(config.window_mode)) {
        LOG_WARNING(atom::log::backend::sdl3::Render,
                    "Requested window mode '" + std::string{window::ToString(config.window_mode)} +
                        "' is unavailable; continuing in '" + std::string{window::ToString(window_.GetWindowMode())} +
                        "'");
    }

    if (!device_.Initialize(window_.GetNativeWindow(), config.driver, config.validation)) {
        window_.Shutdown();
        return false;
    }

    // Present mode after claiming, because the swapchain only exists once the
    // window is claimed. SDL starts it on VSYNC, so only a different request
    // needs a call; a rejected mode keeps VSync and is reported, never silently
    // swapped for another one.
    if (config.present_mode != render::PresentMode::VSync && !device_.SetPresentMode(config.present_mode)) {
        LOG_WARNING(atom::log::backend::sdl3::Render, "Requested present mode '" +
                                                          std::string{render::ToString(config.present_mode)} +
                                                          "' is unavailable; keeping vertical sync");
    }
    return true;
}
auto SDLGPUBackend::Shutdown() -> void {
    device_.Shutdown();
    window_.Shutdown();
}
auto SDLGPUBackend::Window() -> window::IWindow& {
    return window_;
}
auto SDLGPUBackend::Device() -> render::IRenderDevice& {
    return device_;
}
auto SDLGPUBackend::TimeSource() -> backend::ITimeSource& {
    return time_source_;
}

} // namespace atom::backend::sdlgpu

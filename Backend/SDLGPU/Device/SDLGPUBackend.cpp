/**
  * @file           : SDLGPUBackend.cpp
  * @author         : Romi Brooks
  * @brief          : Implements SDL_GPU render backend lifecycle orchestration.
  * @attention      : SDL_GPU native objects remain private to this backend.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDLGPUBackend.hpp"

namespace atom::backend::sdlgpu {

SDLGPUBackend::~SDLGPUBackend() {
    Shutdown();
}
auto SDLGPUBackend::Initialize(const std::string& title, const algo::Vec2 resolution) -> bool {
    if (!window_.Initialize(title, resolution))
        return false;
    if (!device_.Initialize(window_.GetNativeWindow())) {
        window_.Shutdown();
        return false;
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

} // namespace atom::backend::sdlgpu

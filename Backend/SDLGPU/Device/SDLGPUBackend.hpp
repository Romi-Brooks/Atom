/**
  * @file           : SDLGPUBackend.hpp
  * @author         : Romi Brooks
  * @brief          : SDL_GPU implementation of the render backend contract.
  * @attention      : SDL_GPU native objects remain private to this backend.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDLGPU_BACKEND_HPP
#define ATOM_BACKEND_SDLGPU_BACKEND_HPP

#include <Backend/Contracts/Render/IRenderBackend.hpp>
#include <Backend/SDLGPU/Device/SDLGPUDevice.hpp>
#include <Backend/SDL3/Window/SDL3Window.hpp>

namespace atom::backend::sdlgpu {

class SDLGPUBackend final : public render::IRenderBackend {
    public:
        ~SDLGPUBackend() override;
        auto Initialize(const std::string& title, algo::Vec2 resolution) -> bool override;
        auto Shutdown() -> void override;
        [[nodiscard]] auto Window() -> window::IWindow& override;
        [[nodiscard]] auto Device() -> render::IRenderDevice& override;

    private:
        sdl3::SDL3Window window_;
        SDLGPUDevice device_;
};

} // namespace atom::backend::sdlgpu

#endif

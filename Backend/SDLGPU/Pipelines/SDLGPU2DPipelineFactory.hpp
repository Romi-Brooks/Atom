/**
  * @file           : SDLGPU2DPipelineFactory.hpp
  * @author         : Romi Brooks
  * @brief          : Factory for built-in SDL_GPU Renderer2D pipelines.
  * @attention      : Post-process pipelines are optional; Primitive2D is required.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDLGPU_2D_PIPELINE_FACTORY_HPP
#define ATOM_BACKEND_SDLGPU_2D_PIPELINE_FACTORY_HPP

#include <filesystem>

#include <SDL3/SDL.h>

namespace atom::backend::sdlgpu {

// Built-in SDL_GPU pipelines used by Renderer2D.  The factory owns creation
// details while SDLGPUDevice remains responsible for lifetime and frame state.
struct SDLGPU2DPipelineSet {
    SDL_GPUGraphicsPipeline* primitive = nullptr;
    SDL_GPUGraphicsPipeline* chromatic_aberration = nullptr;
    SDL_GPUGraphicsPipeline* glitch = nullptr;
    SDL_GPUGraphicsPipeline* gaussian_blur = nullptr;
};

// Primitive2D is required. Post-process pipelines are optional; callers may
// still render unprocessed geometry when one of those shaders is unavailable.
auto CreateSDLGPU2DPipelineSet(SDL_GPUDevice* device, SDL_GPUTextureFormat target_format,
                               const std::filesystem::path& shader_root, SDLGPU2DPipelineSet& out) -> bool;

auto ReleaseSDLGPU2DPipelineSet(SDL_GPUDevice* device, SDLGPU2DPipelineSet& pipelines) -> void;

} // namespace atom::backend::sdlgpu

#endif // ATOM_BACKEND_SDLGPU_2D_PIPELINE_FACTORY_HPP

/**
  * @file           : SDLGPUShaderLoader.hpp
  * @author         : Romi Brooks
  * @brief          : Runtime selection of precompiled SDL_GPU shader variants.
  * @attention      : Selects DXIL, SPIR-V or MSL according to device capabilities.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDLGPU_SHADER_LOADER_HPP
#define ATOM_BACKEND_SDLGPU_SHADER_LOADER_HPP

#include <cstdint>
#include <filesystem>
#include <string_view>

#include <SDL3/SDL.h>

namespace atom::backend::sdlgpu {

// Runtime loader for precompiled shader variants. It is backend-specific on
// purpose: Render/Shader owns source/manifests, while this class translates a
// package into SDL_GPUShader objects and selects a supported format.
auto LoadSDLGPUShader(SDL_GPUDevice* device, const std::filesystem::path& root, std::string_view name,
                      SDL_GPUShaderStage stage, uint32_t uniform_buffers, uint32_t samplers) -> SDL_GPUShader*;

} // namespace atom::backend::sdlgpu

#endif // ATOM_BACKEND_SDLGPU_SHADER_LOADER_HPP

/**
  * @file           : SDLGPUValidationScene.hpp
  * @author         : Romi Brooks
  * @brief          : SDL_GPU validation scene shared by render smoke examples.
  * @attention      : Example-only validation; not part of the public renderer API.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_EXAMPLE_RENDER_SDLGPU_VALIDATION_SCENE_HPP
#define ATOM_EXAMPLE_RENDER_SDLGPU_VALIDATION_SCENE_HPP

#include <filesystem>

#include <SDL3/SDL.h>

namespace atom::backend::sdlgpu {
class SDLGPUDevice;
} // namespace atom::backend::sdlgpu

namespace atom::example::render {

class SDLGPUValidationScene final {
    public:
        ~SDLGPUValidationScene();
        auto Initialize(atom::backend::sdlgpu::SDLGPUDevice& device,
                        const std::filesystem::path& shader_root) -> bool;
        auto Render(float elapsed_seconds) -> bool;
        auto Shutdown() -> void;

    private:
        auto CreatePipelines(const std::filesystem::path& shader_root) -> bool;
        auto CreateResources() -> bool;
        auto EnsureDepthTexture(uint32_t width, uint32_t height) -> bool;

        atom::backend::sdlgpu::SDLGPUDevice* owner_ = nullptr;
        SDL_GPUDevice* device_ = nullptr;
        SDL_GPUGraphicsPipeline* triangle_pipeline_ = nullptr;
        SDL_GPUGraphicsPipeline* mesh_pipeline_ = nullptr;
        SDL_GPUGraphicsPipeline* sprite_pipeline_ = nullptr;
        SDL_GPUBuffer* vertex_buffer_ = nullptr;
        SDL_GPUBuffer* index_buffer_ = nullptr;
        SDL_GPUTexture* sprite_texture_ = nullptr;
        SDL_GPUTexture* depth_texture_ = nullptr;
        SDL_GPUSampler* sampler_ = nullptr;
        uint32_t depth_width_ = 0;
        uint32_t depth_height_ = 0;
};

} // namespace atom::example::render

#endif // ATOM_EXAMPLE_RENDER_SDLGPU_VALIDATION_SCENE_HPP

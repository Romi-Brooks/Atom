/**
  * @file           : SDLGPUDevice.hpp
  * @author         : Romi Brooks
  * @brief          : SDL_GPU implementation of the render device contract.
  * @attention      : SDL_GPU native objects remain private to this backend.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDLGPU_DEVICE_HPP
#define ATOM_BACKEND_SDLGPU_DEVICE_HPP

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Render/IRender2DContext.hpp>
#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include "../Pipelines/SDLGPU2DPipelineFactory.hpp"

namespace atom::backend::sdlgpu {

// Texture bookkeeping: SDL_GPU does not expose a size query for textures, so
// the 2D context stores the dimensions it was created with.
struct SDLGPU2DTextureEntry {
        SDL_GPUTexture* texture = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
};

class SDLGPUDevice final : public render::IRenderDevice, public render::IRender2DContext {
    public:
        ~SDLGPUDevice() override;
        auto Initialize(SDL_Window* window) -> bool;
        auto Shutdown() -> void;
        [[nodiscard]] auto BeginFrame() -> bool override;
        auto Clear(const render::Color& color) -> void override;
        auto EndFrame() -> void override;
        auto HandleResize(uint32_t width, uint32_t height) -> void override;
        auto SetVSync(bool enabled) -> bool override;
        [[nodiscard]] auto IsVSyncEnabled() const -> bool override;
        [[nodiscard]] auto GetOutputSize() const -> algo::Vec2 override;
        [[nodiscard]] auto GetBackendInfo() const -> const render::RenderBackendInfo& override;

        // Backend-internal encoding extension used by SDL_GPU resource and
        // renderer implementations. Never expose these handles through RHI.
        [[nodiscard]] auto GetNativeDevice() const -> SDL_GPUDevice*;
        [[nodiscard]] auto GetNativeCommandBuffer() const -> SDL_GPUCommandBuffer*;
        [[nodiscard]] auto GetNativeSwapchainTexture() const -> SDL_GPUTexture*;
        auto MarkFrameEncoded() -> void;

        // Frame-pass coordination for in-frame encoders (scene, overlay):
        // the first encoder must CLEAR the swapchain, later ones must LOAD.
        [[nodiscard]] auto IsFrameEncoded() const -> bool {
            return frame_encoded_;
        }
        [[nodiscard]] auto GetClearColor() const -> const render::Color& {
            return clear_color_;
        }

        // --- render::IRender2DContext ---
        auto Initialize2D(const std::filesystem::path& shader_root) -> bool override;
        [[nodiscard]] auto CreateTexture2D(uint32_t width, uint32_t height) -> render::Texture2D override;
        auto UpdateTexture2D(render::Texture2D texture, const void* pixels, uint32_t pitch_bytes) -> bool override;
        auto DestroyTexture2D(render::Texture2D texture) -> void override;
        [[nodiscard]] auto CreateSampler2D(const render::Sampler2DDesc& desc) -> render::Sampler2D override;
        auto DestroySampler2D(render::Sampler2D sampler) -> void override;
        auto SetPostProcess2D(const render::PostProcess2DParams& params) -> void override;
        auto ResolvePostProcess2D() -> bool override;
        auto Submit2DFrame(const render::Render2DFrame& frame) -> bool override;
        [[nodiscard]] auto GetMax2DVertices() const -> uint32_t override;
        [[nodiscard]] auto GetMax2DIndices() const -> uint32_t override;

    private:
        auto Release2DResources() -> void;
        auto EnsurePostProcessTarget(uint32_t width, uint32_t height) -> bool;
        auto EncodePostProcess(SDL_GPUTexture* source, uint32_t width, uint32_t height) -> bool;

        SDL_GPUDevice* device_ = nullptr;
        SDL_Window* window_ = nullptr;
        SDL_GPUCommandBuffer* command_buffer_ = nullptr;
        SDL_GPUTexture* swapchain_texture_ = nullptr;
        uint32_t frame_width_ = 0;
        uint32_t frame_height_ = 0;
        render::Color clear_color_ = render::Color::Black();
        render::RenderBackendInfo info_{};
        bool vsync_enabled_ = true;
        bool frame_encoded_ = false;

        // 2D context state (owned by this device; see Encoding/SDLGPU2D.cpp).
        std::filesystem::path shader_root_2d_{};
        SDL_GPUGraphicsPipeline* pipeline_2d_ = nullptr;
        SDL_GPUGraphicsPipeline* pipeline_postprocess_ = nullptr;
        SDL_GPUGraphicsPipeline* pipeline_glitch_ = nullptr;
        SDL_GPUGraphicsPipeline* pipeline_blur_ = nullptr;
        SDLGPU2DPipelineSet pipeline_set_{};
        bool pipeline_2d_init_failed_ = false;
        SDL_GPUBuffer* vertex_buffer_2d_ = nullptr;
        SDL_GPUBuffer* index_buffer_2d_ = nullptr;
        uint32_t vertex_capacity_2d_ = 0;
        uint32_t index_capacity_2d_ = 0;
        uint64_t next_texture_2d_ = 1;
        uint64_t next_sampler_2d_ = 1;
        std::unordered_map<render::Texture2D, SDLGPU2DTextureEntry> textures_2d_{};
        std::unordered_map<render::Sampler2D, SDL_GPUSampler*> samplers_2d_{};
        SDL_GPUTexture* postprocess_texture_ = nullptr;
        uint32_t postprocess_width_ = 0;
        uint32_t postprocess_height_ = 0;
        render::PostProcess2DParams postprocess_params_{};
        bool postprocess_target_encoded_ = false;
        // Transfer buffers referenced by the still-open frame command buffer.
        // SDL_GPU requires them to stay alive until that buffer is submitted,
        // so they are released right after EndFrame() submits (or on Shutdown).
        std::vector<SDL_GPUTransferBuffer*> pending_transfers_2d_{};
};

} // namespace atom::backend::sdlgpu

#endif

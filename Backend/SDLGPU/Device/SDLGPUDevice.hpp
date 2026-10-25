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

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Render/IRender2DContext.hpp>
#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Render/RenderBackendConfig.hpp>
#include <Backend/SDLGPU/Pipelines/SDLGPU2DPipelineFactory.hpp>

namespace atom::backend::sdlgpu {

// Texture bookkeeping: SDL_GPU does not expose a size query for textures, so
// the 2D context stores the dimensions it was created with.
struct SDLGPU2DTextureEntry {
        SDL_GPUTexture* texture = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
};

// Persistent upload staging buffer. The 2D encoder creates one per in-flight
// submission instead of allocating and freeing a transfer buffer every segment
// of every frame (a high-frequency alloc/free that fragments GPU allocators).
struct SDLGPUUploadBufferEntry {
        SDL_GPUTransferBuffer* buffer = nullptr;
        uint32_t capacity = 0;
};

class SDLGPUDevice final : public render::IRenderDevice, public render::IRender2DContext {
    public:
        ~SDLGPUDevice() override;
        // `driver` selects which graphics API SDL_GPU runs on. Auto lets SDL
        // choose; an explicit driver that the build or the machine cannot
        // provide falls back to Auto with a warning instead of failing startup.
        // `validation` decides whether the API's debug layer comes up; Auto
        // follows the build type.
        auto Initialize(SDL_Window* window, backend::GpuDriver driver = backend::GpuDriver::Auto,
                        backend::ValidationMode validation = backend::ValidationMode::Auto) -> bool;
        auto Shutdown() -> void;
        [[nodiscard]] auto BeginFrame() -> bool override;
        auto Clear(const render::Color& color) -> void override;
        auto EndFrame() -> void override;
        auto HandleResize(uint32_t width, uint32_t height) -> void override;
        auto SetPresentMode(render::PresentMode mode) -> bool override;
        [[nodiscard]] auto GetPresentMode() const -> render::PresentMode override;
        [[nodiscard]] auto SupportedPresentModes() const -> std::uint8_t override;
        [[nodiscard]] auto GetOutputSize() const -> algo::Vec2 override;
        [[nodiscard]] auto GetBackendInfo() const -> const render::RenderBackendInfo& override;
        [[nodiscard]] auto GetFrameTiming() const -> render::FrameTiming override;
        [[nodiscard]] auto IsPresentRefreshPaced() const -> bool override;

        // Drops the cached present-mode capability mask. Called when the surface
        // can have changed (resize, fullscreen transition). Public so a window
        // mode change can force a re-query without recreating the device.
        auto InvalidatePresentModeCache() -> void;

        // Backend-internal encoding extension used by SDL_GPU resource and
        // renderer implementations. Never expose these handles through RHI.
        [[nodiscard]] auto GetNativeDevice() const -> SDL_GPUDevice*;
        [[nodiscard]] auto GetNativeCommandBuffer() const -> SDL_GPUCommandBuffer*;
        [[nodiscard]] auto GetNativeSwapchainTexture() const -> SDL_GPUTexture*;
        auto MarkFrameEncoded() -> void;

        // Frame-pass coordination for in-frame encoders (scene, overlay):
        // the first encoder must CLEAR the swapchain, later ones must LOAD.
        [[nodiscard]] auto IsFrameEncoded() const -> bool {
            return CurrentFrame().encoded;
        }
        [[nodiscard]] auto GetClearColor() const -> const render::Color& {
            return clear_color_;
        }

        // --- render::IRender2DContext ---
        auto Initialize2D(const std::filesystem::path& shader_root) -> bool override;
        [[nodiscard]] auto CreateTexture2D(uint32_t width, uint32_t height) -> render::Texture2D override;
        auto UpdateTexture2D(render::Texture2D texture, const void* pixels, uint32_t pitch_bytes) -> bool override;
        auto UpdateTexture2DRegion(render::Texture2D texture, uint32_t x, uint32_t y, uint32_t width,
                                   uint32_t height, const void* pixels, uint32_t source_pitch_bytes) -> bool override;
        auto DestroyTexture2D(render::Texture2D texture) -> void override;
        [[nodiscard]] auto CreateSampler2D(const render::Sampler2DDesc& desc) -> render::Sampler2D override;
        auto DestroySampler2D(render::Sampler2D sampler) -> void override;
        auto SetPostProcess2D(const render::PostProcess2DParams& params) -> void override;
        auto ResolvePostProcess2D() -> bool override;
        auto Submit2DFrame(const render::Render2DFrame& frame) -> bool override;
        [[nodiscard]] auto GetMax2DVertices() const -> uint32_t override;
        [[nodiscard]] auto GetMax2DIndices() const -> uint32_t override;

    private:
        // Number of frames the CPU may be ahead of the GPU. 2 keeps latency
        // low while removing the per-frame CPU<->GPU stall.
        static constexpr std::size_t kMaxFramesInFlight = 2;

    public:
        // --- submit-phase diagnostics ---------------------------------------
        // Per-frame accounting of where the record window spends its time, for
        // backends whose per-call cost differs wildly: SDL_GPU's Vulkan encoder
        // measured 34x the D3D12 one on an identical command stream (10.0 ms vs
        // 0.30 ms of CPU record), which is invisible in a total.
        //
        // Filled by the encoders (2D, post-process, ImGui overlay) and reported
        // once every kPhaseReportFrames frames from EndFrame(), so the panel shows
        // a steady average instead of a per-frame flicker.
        // The contract's diagnostic struct; this backend fills every field.
        using PhaseStats = render::EncoderPhases;

        [[nodiscard]] auto Phases() -> PhaseStats& {
            return phases_;
        }

        // Averaged over the last completed sample window: what the Render panel
        // displays. There is no logging path -- these numbers are a live readout,
        // not a log line, so a profiling session costs nothing extra.
        [[nodiscard]] auto GetEncoderPhases() const -> render::EncoderPhases override;
        [[nodiscard]] auto GetEncoderPhaseFrames() const -> int override;

    private:
        // Frames per aggregated phase sample; ~2 s at 60 fps.
        static constexpr int kPhaseReportFrames = 120;

        PhaseStats phases_{};
        PhaseStats phase_snapshot_{};
        int phase_snapshot_frames_ = 0;
        int phase_frames_ = 0;

        // Everything that is written once per frame and read by the GPU while
        // that frame is in flight. Fence-ordered so the CPU can encode frame
        // N+1 while the GPU still executes frame N.
        struct FrameContext {
                SDL_GPUCommandBuffer* command_buffer = nullptr;
                SDL_GPUTexture* swapchain_texture = nullptr;
                SDL_GPUFence* fence = nullptr;
                uint32_t swap_width = 0;
                uint32_t swap_height = 0;
                bool encoded = false;
                bool postprocess_encoded = false;
                SDL_GPUBuffer* vertex_buffer = nullptr;
                SDL_GPUBuffer* index_buffer = nullptr;
                uint32_t vertex_capacity = 0;
                uint32_t index_capacity = 0;
                SDL_GPUTexture* postprocess_texture = nullptr;
                uint32_t postprocess_width = 0;
                uint32_t postprocess_height = 0;
                // Geometry upload staging: one slot per in-frame segment.
                std::vector<SDLGPUUploadBufferEntry> upload_pool;
                std::size_t upload_cursor = 0;
                // Transfer buffers recorded into this frame's command buffer
                // (texture uploads); released once the frame's fence completes.
                std::vector<SDL_GPUTransferBuffer*> pending_transfers;
        };

        [[nodiscard]] auto CurrentFrame() -> FrameContext& {
            return frames_[frame_index_];
        }
        [[nodiscard]] auto CurrentFrame() const -> const FrameContext& {
            return frames_[frame_index_];
        }

        auto Release2DResources() -> void;
        auto EnsurePostProcessTarget(uint32_t width, uint32_t height) -> bool;
        auto EncodePostProcess(SDL_GPUTexture* source, uint32_t width, uint32_t height) -> bool;
        // Returns a transfer buffer with >= min_bytes capacity for the current
        // frame's geometry upload, growing the persistent pool only when needed.
        [[nodiscard]] auto AcquireUploadBuffer(uint32_t min_bytes) -> SDL_GPUTransferBuffer*;

        SDL_GPUDevice* device_ = nullptr;
        SDL_Window* window_ = nullptr;
        render::Color clear_color_ = render::Color::Black();
        render::RenderBackendInfo info_{};
        // The mode the swapchain is actually running (SDL starts every claimed
        // window on VSYNC, so this is VSYNC until SetPresentMode succeeds).
        render::PresentMode present_mode_ = render::PresentMode::VSync;

        std::array<FrameContext, kMaxFramesInFlight> frames_{};
        std::size_t frame_index_ = 0;

        // Frame timing. Every value is measured where the cost is actually paid:
        // the frame period here, the GPU backpressure inside the fence wait, the
        // encode cost between BeginFrame() and EndFrame(). `last_fence_done`
        // gives the GPU its own clock without needing timestamp queries.
        using Clock = std::chrono::steady_clock;
        render::FrameTiming timing_{};
        Clock::time_point last_begin_{};
        Clock::time_point encode_start_{};
        Clock::time_point last_fence_done_{};
        bool has_fence_sample_ = false;
        bool has_begin_sample_ = false;

        // Cached present-mode capabilities (see SupportedPresentModes): querying
        // them costs a driver round-trip on Vulkan, and UI asks per frame.
        mutable std::uint8_t supported_present_modes_ = 0;
        mutable bool supported_present_modes_valid_ = false;

        // 2D context state (shared across frames; see Encoding/SDLGPU2D.cpp).
        std::filesystem::path shader_root_2d_{};
        SDL_GPUGraphicsPipeline* pipeline_2d_ = nullptr;
        SDL_GPUGraphicsPipeline* pipeline_postprocess_ = nullptr;
        SDL_GPUGraphicsPipeline* pipeline_glitch_ = nullptr;
        SDL_GPUGraphicsPipeline* pipeline_blur_ = nullptr;
        SDLGPU2DPipelineSet pipeline_set_{};
        bool pipeline_2d_init_failed_ = false;
        uint64_t next_texture_2d_ = 1;
        uint64_t next_sampler_2d_ = 1;
        std::unordered_map<render::Texture2D, SDLGPU2DTextureEntry> textures_2d_{};
        std::unordered_map<render::Sampler2D, SDL_GPUSampler*> samplers_2d_{};
        render::PostProcess2DParams postprocess_params_{};
};

} // namespace atom::backend::sdlgpu

#endif // ATOM_BACKEND_SDLGPU_DEVICE_HPP

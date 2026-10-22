/**
  * @file           : SDLGPUDevice.cpp
  * @author         : Romi Brooks
  * @brief          : Implements SDL_GPU device, frame and resource operations.
  * @attention      : SDL_GPU native objects remain private to this backend.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDLGPUDevice.hpp"

#include <string>
#include <cstdlib>

#include <Log/LogSystem.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>

namespace atom::backend::sdlgpu {

SDLGPUDevice::~SDLGPUDevice() {
    Shutdown();
}

auto SDLGPUDevice::Initialize(SDL_Window* window) -> bool {
    if (!window) {
        LOG_ERROR(atom::log::backend::sdl3::Render, "SDL_GPU initialization requires a valid SDL window");
        return false;
    }
    if (device_) {
        LOG_WARNING(atom::log::backend::sdl3::Render, "SDL_GPU device is already initialized");
        return window_ == window;
    }
    constexpr auto formats = static_cast<SDL_GPUShaderFormat>(SDL_GPU_SHADERFORMAT_SPIRV
#if ATOM_SHADER_HAS_DXIL
                                                              | SDL_GPU_SHADERFORMAT_DXIL
#endif
#if ATOM_SHADER_HAS_MSL
                                                              | SDL_GPU_SHADERFORMAT_MSL
#endif
    );
#ifndef NDEBUG
    constexpr bool debugMode = true;
#else
    constexpr bool debugMode = false;
#endif
    // Allow forcing a specific GPU driver via ATOM_GPU_DRIVER=vulkan|direct3d12.
    // Useful for comparing backend behavior without recompiling.
    const char* forcedDriver = std::getenv("ATOM_GPU_DRIVER");
    if (forcedDriver && forcedDriver[0] != '\0') {
        LOG_INFO(atom::log::backend::sdl3::Render,
                 "Forcing SDL_GPU driver via ATOM_GPU_DRIVER: " + std::string{forcedDriver});
    }
    device_ = SDL_CreateGPUDevice(formats, debugMode, forcedDriver);
    if (!device_) {
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "SDL_CreateGPUDevice failed: " + std::string{SDL_GetError()});
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device_, window)) {
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "SDL_ClaimWindowForGPUDevice failed: " + std::string{SDL_GetError()});
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
        return false;
    }
    window_ = window;
    info_.api = SDL_GetGPUDeviceDriver(device_) ? SDL_GetGPUDeviceDriver(device_) : "unknown";
    const SDL_PropertiesID properties = SDL_GetGPUDeviceProperties(device_);
    info_.device = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown");
    info_.driver = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "unknown");
    info_.shader_formats = static_cast<uint32_t>(SDL_GetGPUShaderFormats(device_));
    info_.swapchain_format = static_cast<uint32_t>(SDL_GetGPUSwapchainTextureFormat(device_, window_));
    LOG_INFO(atom::log::backend::sdl3::Render,
             "SDL_GPU initialized (api=" + info_.api + ", device=" + info_.device + ", driver=" + info_.driver +
                 ", shader_formats=" + std::to_string(info_.shader_formats) +
                 ", swapchain_format=" + std::to_string(info_.swapchain_format) + ")");
    return true;
}

auto SDLGPUDevice::Shutdown() -> void {
    // Cancel any unsubmitted command buffer; present the one that owns a
    // swapchain texture so the window's last acquired image is returned.
    for (auto& frame : frames_) {
        if (!frame.command_buffer)
            continue;
        if (frame.swapchain_texture)
            SDL_SubmitGPUCommandBuffer(frame.command_buffer);
        else
            SDL_CancelGPUCommandBuffer(frame.command_buffer);
        frame.command_buffer = nullptr;
        frame.swapchain_texture = nullptr;
    }
    // Wait out every in-flight fence so no resource is released under the GPU.
    for (auto& frame : frames_) {
        if (frame.fence) {
            if (device_)
                SDL_WaitForGPUFences(device_, true, &frame.fence, 1);
            if (frame.fence)
                SDL_ReleaseGPUFence(device_, frame.fence);
            frame.fence = nullptr;
        }
    }
    Release2DResources();
    if (device_ && window_)
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
    if (device_)
        SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
    window_ = nullptr;
}

auto SDLGPUDevice::BeginFrame() -> bool {
    if (!device_) {
        LOG_WARNING(atom::log::backend::sdl3::Render, "SDL_GPU BeginFrame called before initialization");
        return false;
    }
    auto& frame = CurrentFrame();
    if (frame.command_buffer) {
        LOG_WARNING(atom::log::backend::sdl3::Render, "SDL_GPU BeginFrame called while another frame is active");
        return false;
    }

    // This context was last used two frames ago. Wait for its fence so its
    // persistent staging buffers, geometry buffers and pending transfers can be
    // reused without racing the GPU.
    if (frame.fence) {
        SDL_WaitForGPUFences(device_, true, &frame.fence, 1);
        SDL_ReleaseGPUFence(device_, frame.fence);
        frame.fence = nullptr;
        for (auto* transfer : frame.pending_transfers) {
            if (transfer)
                SDL_ReleaseGPUTransferBuffer(device_, transfer);
        }
        frame.pending_transfers.clear();
    }

    frame.command_buffer = SDL_AcquireGPUCommandBuffer(device_);
    if (!frame.command_buffer) {
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "SDL_AcquireGPUCommandBuffer failed: " + std::string{SDL_GetError()});
        return false;
    }
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(frame.command_buffer, window_, &frame.swapchain_texture,
                                               &frame.swap_width, &frame.swap_height)) {
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "Failed to acquire SDL_GPU swapchain: " + std::string{SDL_GetError()});
        SDL_CancelGPUCommandBuffer(frame.command_buffer);
        frame.command_buffer = nullptr;
        return false;
    }
    if (!frame.swapchain_texture) {
        SDL_SubmitGPUCommandBuffer(frame.command_buffer);
        frame.command_buffer = nullptr;
        return false;
    }
    frame.encoded = false;
    frame.postprocess_encoded = false;
    frame.upload_cursor = 0;
    return true;
}

auto SDLGPUDevice::AcquireUploadBuffer(const uint32_t min_bytes) -> SDL_GPUTransferBuffer* {
    if (!device_ || min_bytes == 0)
        return nullptr;
    auto& frame = CurrentFrame();

    // Reuse a pool slot if it already exists and is large enough.
    if (frame.upload_cursor < frame.upload_pool.size()) {
        auto& slot = frame.upload_pool[frame.upload_cursor];
        if (slot.buffer && slot.capacity >= min_bytes) {
            ++frame.upload_cursor;
            return slot.buffer;
        }
        // The slot exists but is too small: replace it with a larger buffer.
        if (slot.buffer) {
            SDL_ReleaseGPUTransferBuffer(device_, slot.buffer);
            slot.buffer = nullptr;
            slot.capacity = 0;
        }
    }

    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, min_bytes, 0};
    auto* buffer = SDL_CreateGPUTransferBuffer(device_, &info);
    if (!buffer)
        return nullptr;

    if (frame.upload_cursor < frame.upload_pool.size()) {
        auto& slot = frame.upload_pool[frame.upload_cursor];
        slot.buffer = buffer;
        slot.capacity = min_bytes;
    } else {
        frame.upload_pool.push_back(SDLGPUUploadBufferEntry{buffer, min_bytes});
    }
    ++frame.upload_cursor;
    return buffer;
}

auto SDLGPUDevice::Clear(const render::Color& color) -> void {
    clear_color_ = color;
}

auto SDLGPUDevice::EndFrame() -> void {
    auto& frame = CurrentFrame();
    if (!frame.command_buffer)
        return;
    if (!frame.swapchain_texture) {
        SDL_CancelGPUCommandBuffer(frame.command_buffer);
        frame.command_buffer = nullptr;
        return;
    }
    if (!frame.encoded) {
        SDL_GPUColorTargetInfo target{};
        target.texture = frame.swapchain_texture;
        target.clear_color = {clear_color_.r / 255.0f, clear_color_.g / 255.0f, clear_color_.b / 255.0f,
                              clear_color_.a / 255.0f};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        if (auto* pass = SDL_BeginGPURenderPass(frame.command_buffer, &target, 1, nullptr)) {
            SDL_EndGPURenderPass(pass);
        } else {
            LOG_ERROR(atom::log::backend::sdl3::Render,
                      "SDL_BeginGPURenderPass failed: " + std::string{SDL_GetError()});
        }
    }
    // Submit and capture a fence: the next time this context is reused, its
    // fence is waited out before any of its persistent resources are touched.
    frame.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(frame.command_buffer);
    if (!frame.fence)
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "SDL_SubmitGPUCommandBufferAndAcquireFence failed: " + std::string{SDL_GetError()});
    frame.command_buffer = nullptr;
    frame.swapchain_texture = nullptr;
    // Advance to the next in-flight context. Its pending transfers stay alive
    // until its fence completes (they are released at the top of BeginFrame).
    frame_index_ = (frame_index_ + 1) % kMaxFramesInFlight;
}

auto SDLGPUDevice::HandleResize(const uint32_t width, const uint32_t height) -> void {
    LOG_DEBUG(atom::log::backend::sdl3::Render,
              "SDL_GPU resize notification: " + std::to_string(width) + "x" + std::to_string(height) +
                  " (swapchain dimensions are acquired per frame)");
}
auto SDLGPUDevice::SetVSync(const bool enabled) -> bool {
    if (!device_ || !window_) {
        LOG_WARNING(atom::log::backend::sdl3::Render, "SDL_GPU SetVSync called before initialization");
        return false;
    }
    const auto mode = enabled ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_MAILBOX;
    if (!SDL_WindowSupportsGPUPresentMode(device_, window_, mode)) {
        if (!enabled && SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                               SDL_GPU_PRESENTMODE_IMMEDIATE)) {
                LOG_ERROR(atom::log::backend::sdl3::Render,
                          "Failed to select SDL_GPU immediate present mode: " + std::string{SDL_GetError()});
                return false;
            }
            vsync_enabled_ = false;
            LOG_INFO(atom::log::backend::sdl3::Render, "SDL_GPU VSync disabled (immediate present mode)");
            return true;
        }
        LOG_WARNING(atom::log::backend::sdl3::Render,
                    std::string{"Requested SDL_GPU present mode is unsupported (VSync="} + (enabled ? "on)" : "off)"));
        return false;
    }
    if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "Failed to update SDL_GPU present mode: " + std::string{SDL_GetError()});
        return false;
    }
    vsync_enabled_ = enabled;
    LOG_INFO(atom::log::backend::sdl3::Render,
             std::string{"SDL_GPU VSync "} + (enabled ? "enabled" : "disabled (mailbox present mode)"));
    return true;
}
auto SDLGPUDevice::IsVSyncEnabled() const -> bool {
    return vsync_enabled_;
}
auto SDLGPUDevice::GetOutputSize() const -> algo::Vec2 {
    const auto& frame = CurrentFrame();
    if (frame.swap_width != 0 && frame.swap_height != 0)
        return {static_cast<float>(frame.swap_width), static_cast<float>(frame.swap_height)};
    int width = 0, height = 0;
    if (window_)
        SDL_GetWindowSizeInPixels(window_, &width, &height);
    return {static_cast<float>(width), static_cast<float>(height)};
}
auto SDLGPUDevice::GetBackendInfo() const -> const render::RenderBackendInfo& {
    return info_;
}
auto SDLGPUDevice::GetNativeDevice() const -> SDL_GPUDevice* {
    return device_;
}
auto SDLGPUDevice::GetNativeCommandBuffer() const -> SDL_GPUCommandBuffer* {
    return CurrentFrame().command_buffer;
}
auto SDLGPUDevice::GetNativeSwapchainTexture() const -> SDL_GPUTexture* {
    return CurrentFrame().swapchain_texture;
}
auto SDLGPUDevice::MarkFrameEncoded() -> void {
    CurrentFrame().encoded = true;
}

} // namespace atom::backend::sdlgpu

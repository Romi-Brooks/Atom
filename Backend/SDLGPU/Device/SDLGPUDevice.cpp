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
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER, "SDL_GPU initialization requires a valid SDL window");
        return false;
    }
    if (device_) {
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER, "SDL_GPU device is already initialized");
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
        LOG_INFO(atom::backend::sdl3::LogChannel::RENDER,
                 "Forcing SDL_GPU driver via ATOM_GPU_DRIVER: " + std::string{forcedDriver});
    }
    device_ = SDL_CreateGPUDevice(formats, debugMode, forcedDriver);
    if (!device_) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_CreateGPUDevice failed: " + std::string{SDL_GetError()});
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device_, window)) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
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
    LOG_INFO(atom::backend::sdl3::LogChannel::RENDER,
             "SDL_GPU initialized (api=" + info_.api + ", device=" + info_.device + ", driver=" + info_.driver +
                 ", shader_formats=" + std::to_string(info_.shader_formats) +
                 ", swapchain_format=" + std::to_string(info_.swapchain_format) + ")");
    return true;
}

auto SDLGPUDevice::Shutdown() -> void {
    if (command_buffer_) {
        if (swapchain_texture_)
            SDL_SubmitGPUCommandBuffer(command_buffer_);
        else
            SDL_CancelGPUCommandBuffer(command_buffer_);
        command_buffer_ = nullptr;
    }
    swapchain_texture_ = nullptr;
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
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER, "SDL_GPU BeginFrame called before initialization");
        return false;
    }
    if (command_buffer_) {
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER, "SDL_GPU BeginFrame called while another frame is active");
        return false;
    }
    command_buffer_ = SDL_AcquireGPUCommandBuffer(device_);
    if (!command_buffer_) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_AcquireGPUCommandBuffer failed: " + std::string{SDL_GetError()});
        return false;
    }
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer_, window_, &swapchain_texture_, &frame_width_,
                                               &frame_height_)) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Failed to acquire SDL_GPU swapchain: " + std::string{SDL_GetError()});
        SDL_CancelGPUCommandBuffer(command_buffer_);
        command_buffer_ = nullptr;
        return false;
    }
    if (!swapchain_texture_) {
        SDL_SubmitGPUCommandBuffer(command_buffer_);
        command_buffer_ = nullptr;
        return false;
    }
    frame_encoded_ = false;
    postprocess_target_encoded_ = false;
    return true;
}

auto SDLGPUDevice::Clear(const render::Color& color) -> void {
    clear_color_ = color;
}

auto SDLGPUDevice::EndFrame() -> void {
    if (!command_buffer_)
        return;
    if (!swapchain_texture_) {
        SDL_CancelGPUCommandBuffer(command_buffer_);
        command_buffer_ = nullptr;
        return;
    }
    if (!frame_encoded_) {
        SDL_GPUColorTargetInfo target{};
        target.texture = swapchain_texture_;
        target.clear_color = {clear_color_.r / 255.0f, clear_color_.g / 255.0f, clear_color_.b / 255.0f,
                              clear_color_.a / 255.0f};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        if (auto* pass = SDL_BeginGPURenderPass(command_buffer_, &target, 1, nullptr)) {
            SDL_EndGPURenderPass(pass);
        } else {
            LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                      "SDL_BeginGPURenderPass failed: " + std::string{SDL_GetError()});
        }
    }
    if (!SDL_SubmitGPUCommandBuffer(command_buffer_))
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_SubmitGPUCommandBuffer failed: " + std::string{SDL_GetError()});
    command_buffer_ = nullptr;
    swapchain_texture_ = nullptr;
    // The frame's command buffer has been submitted: transfer buffers that
    // were recorded into it may now be released.
    if (device_) {
        for (auto* transfer : pending_transfers_2d_) {
            if (transfer)
                SDL_ReleaseGPUTransferBuffer(device_, transfer);
        }
    }
    pending_transfers_2d_.clear();
}

auto SDLGPUDevice::HandleResize(const uint32_t width, const uint32_t height) -> void {
    LOG_DEBUG(atom::backend::sdl3::LogChannel::RENDER,
              "SDL_GPU resize notification: " + std::to_string(width) + "x" + std::to_string(height) +
                  " (swapchain dimensions are acquired per frame)");
}
auto SDLGPUDevice::SetVSync(const bool enabled) -> bool {
    if (!device_ || !window_) {
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER, "SDL_GPU SetVSync called before initialization");
        return false;
    }
    const auto mode = enabled ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_MAILBOX;
    if (!SDL_WindowSupportsGPUPresentMode(device_, window_, mode)) {
        if (!enabled && SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                               SDL_GPU_PRESENTMODE_IMMEDIATE)) {
                LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                          "Failed to select SDL_GPU immediate present mode: " + std::string{SDL_GetError()});
                return false;
            }
            vsync_enabled_ = false;
            LOG_INFO(atom::backend::sdl3::LogChannel::RENDER, "SDL_GPU VSync disabled (immediate present mode)");
            return true;
        }
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER,
                    std::string{"Requested SDL_GPU present mode is unsupported (VSync="} + (enabled ? "on)" : "off)"));
        return false;
    }
    if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Failed to update SDL_GPU present mode: " + std::string{SDL_GetError()});
        return false;
    }
    vsync_enabled_ = enabled;
    LOG_INFO(atom::backend::sdl3::LogChannel::RENDER,
             std::string{"SDL_GPU VSync "} + (enabled ? "enabled" : "disabled (mailbox present mode)"));
    return true;
}
auto SDLGPUDevice::IsVSyncEnabled() const -> bool {
    return vsync_enabled_;
}
auto SDLGPUDevice::GetOutputSize() const -> algo::Vec2 {
    if (frame_width_ != 0 && frame_height_ != 0)
        return {static_cast<float>(frame_width_), static_cast<float>(frame_height_)};
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
    return command_buffer_;
}
auto SDLGPUDevice::GetNativeSwapchainTexture() const -> SDL_GPUTexture* {
    return swapchain_texture_;
}
auto SDLGPUDevice::MarkFrameEncoded() -> void {
    frame_encoded_ = true;
}

} // namespace atom::backend::sdlgpu

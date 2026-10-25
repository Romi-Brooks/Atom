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

namespace {

// The driver name SDL_CreateGPUDevice expects, or nullptr for "you pick".
auto ToSdlDriverName(const atom::backend::GpuDriver driver) -> const char* {
    switch (driver) {
    case atom::backend::GpuDriver::Auto:
        return nullptr;
    case atom::backend::GpuDriver::Direct3D12:
        return "direct3d12";
    case atom::backend::GpuDriver::Vulkan:
        return "vulkan";
    case atom::backend::GpuDriver::Metal:
        return "metal";
    }
    return nullptr;
}

auto ToSdlPresentMode(const render::PresentMode mode) -> SDL_GPUPresentMode {
    switch (mode) {
    case render::PresentMode::VSync:
        return SDL_GPU_PRESENTMODE_VSYNC;
    case render::PresentMode::Mailbox:
        return SDL_GPU_PRESENTMODE_MAILBOX;
    case render::PresentMode::Immediate:
        return SDL_GPU_PRESENTMODE_IMMEDIATE;
    }
    return SDL_GPU_PRESENTMODE_VSYNC;
}

} // namespace

SDLGPUDevice::~SDLGPUDevice() {
    Shutdown();
}

auto SDLGPUDevice::Initialize(SDL_Window* window, const atom::backend::GpuDriver driver,
                              const atom::backend::ValidationMode validation) -> bool {
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
    // Validation: Auto follows the build type, so a Debug binary validates and a
    // shipped one does not. The override exists for profiling runs -- validation
    // changes frame time by an order of magnitude, and a frame-rate number taken
    // with it on is not a measurement of the engine.
    const bool debugMode = [validation] {
        switch (validation) {
        case atom::backend::ValidationMode::On:
            return true;
        case atom::backend::ValidationMode::Off:
            return false;
        case atom::backend::ValidationMode::Auto:
            break;
        }
#ifndef NDEBUG
        return true;
#else
        return false;
#endif
    }();
    LOG_INFO(atom::log::backend::sdl3::Render,
             "SDL_GPU validation layer: " + std::string{debugMode ? "on" : "off"} + " (requested: " +
                 std::string{atom::backend::ToString(validation)} + ")");
    // Driver selection: an explicit request from RenderBackendConfig.driver,
    // with ATOM_GPU_DRIVER kept as a developer override that wins over code
    // (that is how a driver is probed without recompiling).
    const char* driver_name = ToSdlDriverName(driver);
    const char* forcedDriver = std::getenv("ATOM_GPU_DRIVER");
    if (forcedDriver && forcedDriver[0] != '\0') {
        LOG_INFO(atom::log::backend::sdl3::Render, "Forcing SDL_GPU driver via ATOM_GPU_DRIVER: " +
                                                       std::string{forcedDriver} + " (requested: " +
                                                       std::string{atom::backend::ToString(driver)} + ")");
        driver_name = forcedDriver;
    } else {
        LOG_INFO(atom::log::backend::sdl3::Render,
                 "Requesting SDL_GPU driver: " + std::string{atom::backend::ToString(driver)});
    }
    device_ = SDL_CreateGPUDevice(formats, debugMode, driver_name);
    if (!device_ && driver_name != nullptr) {
        // A driver the machine or this build cannot provide must not make the
        // engine unstartable: warn loudly and let SDL pick instead.
        LOG_WARNING(atom::log::backend::sdl3::Render, "SDL_GPU driver '" + std::string{driver_name} +
                                                          "' unavailable (" + SDL_GetError() +
                                                          "); falling back to SDL's default driver");
        driver_name = nullptr;
        device_ = SDL_CreateGPUDevice(formats, debugMode, nullptr);
    }
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
    // SDL creates the swapchain of a freshly claimed window on VSYNC
    // (SDL_ClaimWindowForGPUDevice docs). The backend turns this into
    // RenderBackendConfig.present_mode right afterwards.
    present_mode_ = render::PresentMode::VSync;
    info_.api = SDL_GetGPUDeviceDriver(device_) ? SDL_GetGPUDeviceDriver(device_) : "unknown";
    const SDL_PropertiesID properties = SDL_GetGPUDeviceProperties(device_);
    info_.device = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown");
    // Only the Vulkan path fills the driver *name*; D3D12 has no equivalent of
    // VkPhysicalDeviceDriverProperties, so it stays empty there. Do not paper
    // over that with a placeholder: "the driver does not report a name" and
    // "we do not know which driver" are different facts, and the version string
    // below is available on both paths anyway.
    info_.driver = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "");
    info_.driver_version = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "");
    info_.shader_formats = static_cast<uint32_t>(SDL_GetGPUShaderFormats(device_));
    info_.swapchain_format = static_cast<uint32_t>(SDL_GetGPUSwapchainTextureFormat(device_, window_));
    LOG_INFO(atom::log::backend::sdl3::Render,
             "SDL_GPU initialized (api=" + info_.api + ", device=" + info_.device +
                 ", driver=" + (info_.driver.empty() ? std::string{"<not reported>"} : info_.driver) +
                 ", driver_version=" +
                 (info_.driver_version.empty() ? std::string{"<not reported>"} : info_.driver_version) +
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
    // A re-initialized device starts on a fresh VSYNC swapchain, so do not let
    // the previous session's mode leak into GetPresentMode().
    present_mode_ = render::PresentMode::VSync;
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

    // Frame period is measured here, not inside a tool: this is the one point
    // every frame passes through, whatever drives it.
    const auto frame_begin = Clock::now();
    if (has_begin_sample_) {
        timing_.frame_ms = std::chrono::duration<double, std::milli>(frame_begin - last_begin_).count();
    } else {
        has_begin_sample_ = true;
    }
    last_begin_ = frame_begin;

    // This context was last used two frames ago. Wait for its fence so its
    // persistent staging buffers, geometry buffers and pending transfers can be
    // reused without racing the GPU.
    if (frame.fence) {
        SDL_WaitForGPUFences(device_, true, &frame.fence, 1);
        const auto waited_until = Clock::now();
        // Time blocked here IS the GPU backpressure for this frame: the CPU has
        // nothing it can do until the GPU finishes frame N-2.
        timing_.cpu_wait_ms = std::chrono::duration<double, std::milli>(waited_until - frame_begin).count();
        if (has_fence_sample_) {
            // Two consecutive fence completions = how fast frames actually
            // retire on the GPU, without needing a timestamp query.
            timing_.gpu_period_ms =
                std::chrono::duration<double, std::milli>(waited_until - last_fence_done_).count();
        }
        last_fence_done_ = waited_until;
        has_fence_sample_ = true;

        SDL_ReleaseGPUFence(device_, frame.fence);
        frame.fence = nullptr;
        for (auto* transfer : frame.pending_transfers) {
            if (transfer)
                SDL_ReleaseGPUTransferBuffer(device_, transfer);
        }
        frame.pending_transfers.clear();
    } else {
        timing_.cpu_wait_ms = 0.0;
        timing_.gpu_period_ms = 0.0;
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
    // Everything from here to EndFrame() is the engine's own CPU cost: recording
    // draw batches, uploading, and submitting command buffers.
    encode_start_ = Clock::now();
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
    // Recording is over: everything up to here is the engine's own CPU cost
    // (building batches, uploads, command encoding).
    const auto record_done = Clock::now();
    timing_.cpu_encode_ms = std::chrono::duration<double, std::milli>(record_done - encode_start_).count();

    // Submit and capture a fence: the next time this context is reused, its
    // fence is waited out before any of its persistent resources are touched.
    //
    // This call also presents, so on a backend whose presentation waits for the
    // vertical blank (Vulkan on Windows, for one) the block lands HERE and not in
    // BeginFrame's fence wait. Timing it separately is what stops a swapchain
    // wait from being misread as "the CPU is slow at submitting".
    frame.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(frame.command_buffer);
    timing_.cpu_submit_ms = std::chrono::duration<double, std::milli>(Clock::now() - record_done).count();
    if (!frame.fence)
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "SDL_SubmitGPUCommandBufferAndAcquireFence failed: " + std::string{SDL_GetError()});
    frame.command_buffer = nullptr;
    frame.swapchain_texture = nullptr;
    // Advance to the next in-flight context. Its pending transfers stay alive
    // until its fence completes (they are released at the top of BeginFrame).
    frame_index_ = (frame_index_ + 1) % kMaxFramesInFlight;
    // Close the sample window every kPhaseReportFrames frames so the panel shows
    // a steady average instead of a per-frame flicker, and the counters (and the
    // two Curves) stay bounded.
    if (++phase_frames_ >= kPhaseReportFrames) {
        phase_snapshot_ = phases_;
        phase_snapshot_frames_ = phase_frames_;
        phases_ = PhaseStats{};
        phase_frames_ = 0;
    }
}

auto SDLGPUDevice::GetEncoderPhases() const -> render::EncoderPhases {
    return phase_snapshot_frames_ > 0 ? phase_snapshot_ : phases_;
}

auto SDLGPUDevice::GetEncoderPhaseFrames() const -> int {
    if (phase_snapshot_frames_ > 0)
        return phase_snapshot_frames_;
    return phase_frames_ > 0 ? phase_frames_ : 1;
}

auto SDLGPUDevice::GetFrameTiming() const -> render::FrameTiming {
    return timing_;
}

auto SDLGPUDevice::IsPresentRefreshPaced() const -> bool {
    switch (present_mode_) {
    case render::PresentMode::VSync:
        return true;
    case render::PresentMode::Mailbox:
        // A driver-side mailbox waits for the vertical blank before swapping, so
        // it throttles the submitter. SDL's D3D12 path has no such mode: it
        // presents with sync interval 0 and no tear flag, which does NOT
        // throttle -- with the exception of fullscreen, where DXGI forces vblank
        // synchronization when tearing is not allowed. Measured on an RTX 4060 /
        // i7-13650HX: windowed Mailbox and windowed Immediate both run uncapped
        // (~2500 fps), fullscreen Mailbox drops to the refresh rate (144), and
        // fullscreen Immediate tears at ~2000.
        if (info_.api != "direct3d12")
            return true;
        return (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
    case render::PresentMode::Immediate:
        // Nominal answer: Immediate does not wait for the blank, so it should not
        // throttle. Do NOT try to be clever here and report "paced" just because
        // a measurement happened to land on the refresh rate: on this machine a
        // Vulkan frame sat at 142 fps with a 0.05 ms submit and a 6.6 ms *record*
        // time, i.e. the frame rate came from CPU cost, not from presentation.
        // Inferring pacing from a frame rate is how a tool starts lying.
        return false;
    }
    return true;
}

auto SDLGPUDevice::HandleResize(const uint32_t width, const uint32_t height) -> void {
    // A resize can rebuild the surface the capability query reads from.
    InvalidatePresentModeCache();
    LOG_DEBUG(atom::log::backend::sdl3::Render,
              "SDL_GPU resize notification: " + std::to_string(width) + "x" + std::to_string(height) +
                  " (swapchain dimensions are acquired per frame)");
}
auto SDLGPUDevice::SetPresentMode(const render::PresentMode mode) -> bool {
    if (!device_ || !window_) {
        LOG_WARNING(atom::log::backend::sdl3::Render, "SDL_GPU SetPresentMode called before initialization");
        return false;
    }
    if (mode == present_mode_)
        return true;

    const auto sdl_mode = ToSdlPresentMode(mode);
    // Never silently substitute a mode: a player who picked "off" and got
    // vsync would report a performance bug that is really a fallback.
    if (!SDL_WindowSupportsGPUPresentMode(device_, window_, sdl_mode)) {
        LOG_WARNING(atom::log::backend::sdl3::Render,
                    "SDL_GPU present mode '" + std::string{render::ToString(mode)} +
                        "' is unsupported on this window/driver; keeping '" +
                        std::string{render::ToString(present_mode_)} + "'");
        return false;
    }
    if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, sdl_mode)) {
        LOG_ERROR(atom::log::backend::sdl3::Render, "Failed to select SDL_GPU present mode '" +
                                                        std::string{render::ToString(mode)} +
                                                        "': " + std::string{SDL_GetError()});
        return false;
    }
    present_mode_ = mode;
    LOG_INFO(atom::log::backend::sdl3::Render,
             "SDL_GPU present mode: '" + std::string{render::ToString(mode)} + "'");
    return true;
}

auto SDLGPUDevice::GetPresentMode() const -> render::PresentMode {
    return present_mode_;
}

auto SDLGPUDevice::SupportedPresentModes() const -> std::uint8_t {
    // VSync is mandatory for every SDL_GPU driver, so an uninitialized device
    // still answers with the one mode that is always legal.
    if (!device_ || !window_)
        return render::ToMask(render::PresentMode::VSync);

    // Cached on purpose. This query is NOT cheap on every backend: SDL's Vulkan
    // implementation calls VULKAN_INTERNAL_QuerySwapchainSupport on each
    // invocation, i.e. vkGetPhysicalDeviceSurfaceCapabilitiesKHR +
    // SurfaceFormatsKHR + SurfacePresentModesKHR plus two malloc/free pairs.
    // Measured: three calls per frame (one per mode) cost 6.9 ms/frame and made
    // a Vulkan build look 34x slower than D3D12, because D3D12's version is a
    // constant. Caching here fixes it for every caller, including UI that asks
    // once per frame out of habit.
    if (supported_present_modes_valid_)
        return supported_present_modes_;

    constexpr render::PresentMode candidates[] = {render::PresentMode::VSync, render::PresentMode::Mailbox,
                                                   render::PresentMode::Immediate};
    std::uint8_t mask = 0;
    for (const auto mode : candidates) {
        if (SDL_WindowSupportsGPUPresentMode(device_, window_, ToSdlPresentMode(mode)))
            mask = static_cast<std::uint8_t>(mask | render::ToMask(mode));
    }
    supported_present_modes_ = mask;
    supported_present_modes_valid_ = true;
    return mask;
}

auto SDLGPUDevice::InvalidatePresentModeCache() -> void {
    // The answer depends on the window's surface, so anything that can replace it
    // (a resize, a fullscreen transition, a new swapchain) invalidates the cache.
    supported_present_modes_valid_ = false;
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

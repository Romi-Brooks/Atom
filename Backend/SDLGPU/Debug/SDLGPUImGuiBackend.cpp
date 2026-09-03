/**
  * @file           : SDLGPUImGuiBackend.cpp
  * @author         : Romi Brooks
  * @brief          : Encodes the SDL_GPU ImGui debugger overlay.
  * @attention      : Debug-only integration; it is not part of the public renderer.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDLGPUImGuiBackend.hpp"

#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Backend/SDL3/Window/ISDL3WindowExtensions.hpp>
#include <Backend/SDLGPU/Device/SDLGPUDevice.hpp>
#include <Log/LogSystem.hpp>

// Third Party
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace atom::backend::sdlgpu {
namespace {

class SDLGPUImGuiBackend;
SDLGPUImGuiBackend* active_backend = nullptr;

// ImGui platform/render glue for the SDL_GPU render backend. All
// SDL3/ImGui/SDL_GPU coupling lives here; the public Debugger and the engine
// headers stay backend-free.
class SDLGPUImGuiBackend final : public debugger::IDebugImGuiBackend {
    public:
        SDLGPUImGuiBackend(SDL_Window* window, SDLGPUDevice& device) : window_(window), device_(device) {}
        ~SDLGPUImGuiBackend() override {
            Shutdown();
        }

        auto Initialize() -> bool override {
            if (initialized_)
                return true;
            if (active_backend && active_backend != this) {
                LOG_ERROR(atom::debugger::LogChannel::IMGUI, "Only one SDL_GPU ImGui debugger can be active at a time");
                return false;
            }
            if (!window_ || !device_.GetNativeDevice()) {
                LOG_ERROR(atom::debugger::LogChannel::IMGUI,
                          "SDL_GPU ImGui initialization requires a valid window and device");
                return false;
            }
            if (!ImGui::CreateContext()) {
                LOG_ERROR(atom::debugger::LogChannel::IMGUI, "ImGui::CreateContext failed");
                return false;
            }
            if (!ImGui_ImplSDL3_InitForSDLGPU(window_)) {
                LOG_ERROR(atom::debugger::LogChannel::IMGUI, "ImGui SDL3 platform backend initialization failed");
                ImGui::DestroyContext();
                return false;
            }

            ImGui_ImplSDLGPU3_InitInfo info{};
            info.Device = device_.GetNativeDevice();
            info.ColorTargetFormat = static_cast<SDL_GPUTextureFormat>(device_.GetBackendInfo().swapchain_format);
            info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
            info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
            info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
            if (!ImGui_ImplSDLGPU3_Init(&info)) {
                LOG_ERROR(atom::debugger::LogChannel::IMGUI, "ImGui SDL_GPU renderer backend initialization failed");
                ImGui_ImplSDL3_Shutdown();
                ImGui::DestroyContext();
                return false;
            }
            initialized_ = true;
            active_backend = this;
            LOG_INFO(atom::debugger::LogChannel::IMGUI, "SDL_GPU ImGui overlay initialized");
            return true;
        }

        auto NewFrame() -> void override {
            if (!initialized_)
                return;
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
        }

        auto Render() -> void override {
            if (!initialized_)
                return;
            ImGui::Render();
            auto* drawData = ImGui::GetDrawData();
            auto* command = device_.GetNativeCommandBuffer();
            auto* swapchain = device_.GetNativeSwapchainTexture();
            if (!command || !swapchain || !drawData || drawData->TotalVtxCount <= 0)
                return;

            // Uploads ImGui vertex/index data into the current frame's command
            // buffer; must run before the render pass that consumes it.
            ImGui_ImplSDLGPU3_PrepareDrawData(drawData, command);

            SDL_GPUColorTargetInfo target{};
            target.texture = swapchain;
            const auto& clear = device_.GetClearColor();
            target.clear_color = {clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f};
            target.load_op = device_.IsFrameEncoded() ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;

            auto* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
            if (!pass) {
                LOG_ERROR(atom::debugger::LogChannel::IMGUI,
                          "SDL_GPU ImGui failed to begin render pass: " + std::string{SDL_GetError()});
                return;
            }
            ImGui_ImplSDLGPU3_RenderDrawData(drawData, command, pass);
            SDL_EndGPURenderPass(pass);
            device_.MarkFrameEncoded();
        }

        auto ProcessRawEvent(const void* rawEvent) -> void override {
            if (!rawEvent)
                return;
            ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(rawEvent));
        }

        auto Shutdown() -> void override {
            if (!initialized_)
                return;
            ImGui_ImplSDLGPU3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            initialized_ = false;
            if (active_backend == this)
                active_backend = nullptr;
            LOG_INFO(atom::debugger::LogChannel::IMGUI, "SDL_GPU ImGui overlay shut down");
        }

    private:
        SDL_Window* window_ = nullptr;
        SDLGPUDevice& device_;
        bool initialized_ = false;
};

} // namespace

auto CreateSDLGPUImGuiBackend(window::IWindow& window, render::IRenderDevice& device)
    -> std::unique_ptr<debugger::IDebugImGuiBackend> {
    auto* windowExtensions = dynamic_cast<sdl3::ISDL3WindowExtensions*>(&window);
    auto* sdlgpuDevice = dynamic_cast<SDLGPUDevice*>(&device);
    if (!windowExtensions || !sdlgpuDevice)
        return nullptr;
    return std::make_unique<SDLGPUImGuiBackend>(windowExtensions->GetNativeWindow(), *sdlgpuDevice);
}

} // namespace atom::backend::sdlgpu

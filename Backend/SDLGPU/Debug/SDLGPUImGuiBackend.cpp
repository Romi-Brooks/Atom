/**
  * @file           : SDLGPUImGuiBackend.cpp
  * @author         : Romi Brooks
  * @brief          : Encodes the SDL_GPU ImGui debugger overlay.
  * @attention      : Debug-only integration; it is not part of the public renderer.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDLGPUImGuiBackend.hpp"

#include <algorithm>

#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Backend/SDLGPU/Device/SDLGPUDevice.hpp>
#include <Log/LogSystem.hpp>

// Third Party
#include <imgui.h>
#include <imgui_impl_sdlgpu3.h>

namespace atom::backend::sdlgpu {
namespace {

class SDLGPUImGuiBackend;
SDLGPUImGuiBackend* active_backend = nullptr;

auto ToImGuiKey(const input::Key key) -> ImGuiKey {
    if (key >= input::Key::A && key <= input::Key::Z)
        return static_cast<ImGuiKey>(ImGuiKey_A + (static_cast<int>(key) - static_cast<int>(input::Key::A)));
    if (key >= input::Key::Digit0 && key <= input::Key::Digit9)
        return static_cast<ImGuiKey>(ImGuiKey_0 + (static_cast<int>(key) - static_cast<int>(input::Key::Digit0)));
    switch (key) {
    case input::Key::Escape:
        return ImGuiKey_Escape;
    case input::Key::Enter:
        return ImGuiKey_Enter;
    case input::Key::Tab:
        return ImGuiKey_Tab;
    case input::Key::Backspace:
        return ImGuiKey_Backspace;
    case input::Key::Space:
        return ImGuiKey_Space;
    case input::Key::Insert:
        return ImGuiKey_Insert;
    case input::Key::Delete:
        return ImGuiKey_Delete;
    case input::Key::Home:
        return ImGuiKey_Home;
    case input::Key::End:
        return ImGuiKey_End;
    case input::Key::PageUp:
        return ImGuiKey_PageUp;
    case input::Key::PageDown:
        return ImGuiKey_PageDown;
    case input::Key::Left:
        return ImGuiKey_LeftArrow;
    case input::Key::Right:
        return ImGuiKey_RightArrow;
    case input::Key::Up:
        return ImGuiKey_UpArrow;
    case input::Key::Down:
        return ImGuiKey_DownArrow;
    case input::Key::F1:
        return ImGuiKey_F1;
    case input::Key::F2:
        return ImGuiKey_F2;
    case input::Key::F3:
        return ImGuiKey_F3;
    case input::Key::F4:
        return ImGuiKey_F4;
    case input::Key::F5:
        return ImGuiKey_F5;
    case input::Key::F6:
        return ImGuiKey_F6;
    case input::Key::F7:
        return ImGuiKey_F7;
    case input::Key::F8:
        return ImGuiKey_F8;
    case input::Key::F9:
        return ImGuiKey_F9;
    case input::Key::F10:
        return ImGuiKey_F10;
    case input::Key::F11:
        return ImGuiKey_F11;
    case input::Key::F12:
        return ImGuiKey_F12;
    case input::Key::LeftShift:
        return ImGuiKey_LeftShift;
    case input::Key::RightShift:
        return ImGuiKey_RightShift;
    case input::Key::LeftControl:
        return ImGuiKey_LeftCtrl;
    case input::Key::RightControl:
        return ImGuiKey_RightCtrl;
    case input::Key::LeftAlt:
        return ImGuiKey_LeftAlt;
    case input::Key::RightAlt:
        return ImGuiKey_RightAlt;
    case input::Key::LeftSuper:
        return ImGuiKey_LeftSuper;
    case input::Key::RightSuper:
        return ImGuiKey_RightSuper;
    case input::Key::CapsLock:
        return ImGuiKey_CapsLock;
    default:
        return ImGuiKey_None;
    }
}

auto ToImGuiMouseButton(const input::MouseButton button) -> int {
    switch (button) {
    case input::MouseButton::Left:
        return 0;
    case input::MouseButton::Right:
        return 1;
    case input::MouseButton::Middle:
        return 2;
    case input::MouseButton::X1:
        return 3;
    case input::MouseButton::X2:
        return 4;
    default:
        return -1;
    }
}

// ImGui platform/render glue for the SDL_GPU render backend. All
// SDL3/ImGui/SDL_GPU coupling lives here; the public Debugger and the engine
// headers stay backend-free.
class SDLGPUImGuiBackend final : public debugger::IDebugImGuiBackend {
    public:
        SDLGPUImGuiBackend(window::IWindow& window, SDLGPUDevice& device) : window_(window), device_(device) {}
        ~SDLGPUImGuiBackend() override {
            Shutdown();
        }

        auto Initialize() -> bool override {
            if (initialized_)
                return true;
            if (active_backend && active_backend != this) {
                LOG_ERROR(atom::log::debugger::ImGui, "Only one SDL_GPU ImGui debugger can be active at a time");
                return false;
            }
            if (!device_.GetNativeDevice()) {
                LOG_ERROR(atom::log::debugger::ImGui,
                          "SDL_GPU ImGui initialization requires a valid window and device");
                return false;
            }
            if (!ImGui::CreateContext()) {
                LOG_ERROR(atom::log::debugger::ImGui, "ImGui::CreateContext failed");
                return false;
            }
            // Debug panels declare their own default geometry (PanelLayout).
            // Never persist window positions to imgui.ini.
            ImGui::GetIO().IniFilename = nullptr;
            ImGui_ImplSDLGPU3_InitInfo info{};
            info.Device = device_.GetNativeDevice();
            info.ColorTargetFormat = static_cast<SDL_GPUTextureFormat>(device_.GetBackendInfo().swapchain_format);
            info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
            info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
            info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
            if (!ImGui_ImplSDLGPU3_Init(&info)) {
                LOG_ERROR(atom::log::debugger::ImGui, "ImGui SDL_GPU renderer backend initialization failed");
                ImGui::DestroyContext();
                return false;
            }
            initialized_ = true;
            active_backend = this;
            LOG_INFO(atom::log::debugger::ImGui, "SDL_GPU ImGui overlay initialized");
            return true;
        }

        auto NewFrame() -> void override {
            if (!initialized_)
                return;
            auto& io = ImGui::GetIO();
            const auto size = window_.GetSize();
            io.DisplaySize = {size.GetX(), size.GetY()};
            const auto now = window_.GetTimeSeconds();
            io.DeltaTime = last_frame_time_ > 0.0 ? static_cast<float>(std::max(now - last_frame_time_, 1.0e-6))
                                                  : 1.0f / 60.0f;
            last_frame_time_ = now;
            ImGui_ImplSDLGPU3_NewFrame();
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
                LOG_ERROR(atom::log::debugger::ImGui,
                          "SDL_GPU ImGui failed to begin render pass: " + std::string{SDL_GetError()});
                return;
            }
            ImGui_ImplSDLGPU3_RenderDrawData(drawData, command, pass);
            SDL_EndGPURenderPass(pass);
            device_.MarkFrameEncoded();
        }

        auto ProcessEvent(const window::IEvent& input_event) -> void override {
            if (!initialized_)
                return;
            auto& io = ImGui::GetIO();
            switch (input_event.type) {
            case window::EventType::KeyPressed:
            case window::EventType::KeyReleased: {
                const auto& key_event = std::get<window::KeyEvent>(input_event.data);
                io.AddKeyEvent(ImGuiMod_Ctrl, input::HasModifier(key_event.modifiers, input::KeyModifiers::Control));
                io.AddKeyEvent(ImGuiMod_Shift, input::HasModifier(key_event.modifiers, input::KeyModifiers::Shift));
                io.AddKeyEvent(ImGuiMod_Alt, input::HasModifier(key_event.modifiers, input::KeyModifiers::Alt));
                io.AddKeyEvent(ImGuiMod_Super, input::HasModifier(key_event.modifiers, input::KeyModifiers::Super));
                const auto key = ToImGuiKey(key_event.key);
                if (key != ImGuiKey_None)
                    io.AddKeyEvent(key, input_event.type == window::EventType::KeyPressed);
                break;
            }
            case window::EventType::MouseMoved: {
                const auto& mouse = std::get<window::MouseEvent>(input_event.data);
                io.AddMousePosEvent(mouse.x, mouse.y);
                break;
            }
            case window::EventType::MouseButtonPressed:
            case window::EventType::MouseButtonReleased: {
                const auto& mouse = std::get<window::MouseEvent>(input_event.data);
                const auto button = ToImGuiMouseButton(mouse.button);
                if (button >= 0)
                    io.AddMouseButtonEvent(button, input_event.type == window::EventType::MouseButtonPressed);
                break;
            }
            case window::EventType::MouseWheel: {
                const auto& wheel = std::get<window::MouseWheelEvent>(input_event.data);
                io.AddMouseWheelEvent(wheel.x, wheel.y);
                break;
            }
            case window::EventType::TextInput:
                io.AddInputCharactersUTF8(std::get<window::TextInputEvent>(input_event.data).text.c_str());
                break;
            case window::EventType::FocusChanged:
                io.AddFocusEvent(std::get<window::FocusEvent>(input_event.data).focused);
                break;
            default:
                break;
            }
        }

        auto WantsMouse() const -> bool override {
            if (!initialized_)
                return false;
            const auto& io = ImGui::GetIO();
            return io.WantCaptureMouse || io.WantCaptureMouseUnlessPopupClose;
        }

        auto Shutdown() -> void override {
            if (!initialized_)
                return;
            ImGui_ImplSDLGPU3_Shutdown();
            ImGui::DestroyContext();
            initialized_ = false;
            if (active_backend == this)
                active_backend = nullptr;
            LOG_INFO(atom::log::debugger::ImGui, "SDL_GPU ImGui overlay shut down");
        }

    private:
        window::IWindow& window_;
        SDLGPUDevice& device_;
        double last_frame_time_ = 0.0;
        bool initialized_ = false;
};

} // namespace

auto CreateSDLGPUImGuiBackend(window::IWindow& window, render::IRenderDevice& device)
    -> std::unique_ptr<debugger::IDebugImGuiBackend> {
    auto* sdlgpuDevice = dynamic_cast<SDLGPUDevice*>(&device);
    if (!sdlgpuDevice)
        return nullptr;
    return std::make_unique<SDLGPUImGuiBackend>(window, *sdlgpuDevice);
}

} // namespace atom::backend::sdlgpu

/**
  * @file           : SDLGPUImGuiBackend.hpp
  * @author         : Romi Brooks
  * @brief          : SDL_GPU ImGui debugger backend interface.
  * @attention      : Debug-only integration; it is not a public UI contract.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_SDLGPU_DEBUG_IMGUI_BACKEND_HPP
#define ATOM_SDLGPU_DEBUG_IMGUI_BACKEND_HPP

#include <memory>

namespace atom::render {
class IRenderDevice;
}
namespace atom::window {
class IWindow;
}
namespace atom::debugger {
class IDebugImGuiBackend;
}

namespace atom::backend::sdlgpu {

// Creates the SDL_GPU ImGui render backend if the device is a SDLGPUDevice;
// input, size, and time are supplied by the backend-neutral IWindow contract.
auto CreateSDLGPUImGuiBackend(window::IWindow& window, render::IRenderDevice& device)
    -> std::unique_ptr<debugger::IDebugImGuiBackend>;

} // namespace atom::backend::sdlgpu

#endif // ATOM_SDLGPU_DEBUG_IMGUI_BACKEND_HPP

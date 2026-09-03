/**
  * @file           : SDLGPUClearSmoke.cpp
  * @author         : Romi Brooks
  * @brief          : SDL_GPU clear, shader and 2D/3D validation example.
  * @attention      : Example-only smoke test; the validation scene is not a public renderer API.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Backend/SDLGPU/Device/SDLGPUBackend.hpp>
#include <Example/Render/SDLGPUValidationScene.hpp>
#include <Backend/SDLGPU/Device/SDLGPUDevice.hpp>

auto main() -> int {
    atom::backend::sdlgpu::SDLGPUBackend backend;
    if (!backend.Initialize("Atom SDL_GPU Stage B", {960.0f, 540.0f}))
        return 1;
    auto& device = static_cast<atom::backend::sdlgpu::SDLGPUDevice&>(backend.Device());
    atom::example::render::SDLGPUValidationScene scene;
    if (!scene.Initialize(device, ATOM_SHADER_OUTPUT_DIR))
        return 2;
    const double start = backend.Window().GetTimeSeconds();
    for (int frame = 0; frame < 120 && backend.Window().IsOpen(); ++frame) {
        while (const auto event = backend.Window().PollEvent()) {
            if (event->type == atom::window::EventType::Closed)
                break;
        }
        if (!device.BeginFrame())
            continue;
        if (!scene.Render(static_cast<float>(backend.Window().GetTimeSeconds() - start)))
            return 3;
        device.EndFrame();
        backend.Window().WaitForNextFrame();
    }
    scene.Shutdown();
    backend.Shutdown();
    return 0;
}

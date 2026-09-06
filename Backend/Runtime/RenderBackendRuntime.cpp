#include "RenderBackendRuntime.hpp"

#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Extension/DebugImGuiBackendRegistry.hpp>
#include <Backend/Extension/RenderBackendRegistry.hpp>
#include <Backend/SDLGPU/Debug/SDLGPUImGuiBackend.hpp>
#include <Backend/SDLGPU/Device/SDLGPUBackend.hpp>

namespace atom::backend {

auto RenderBackendRuntime::GetInstance() -> RenderBackendRuntime& {
    static RenderBackendRuntime instance;
    return instance;
}

auto RenderBackendRuntime::EnsureDefaultRenderBackend() -> void {
    const auto& id = RenderBackendRegistry::kDefaultBackendId;

    auto& registry = RenderBackendRegistry::GetInstance();
    if (!registry.ContainsBackend(id))
        registry.RegisterBackendFactory(id, [] { return std::make_unique<sdlgpu::SDLGPUBackend>(); });

    // The SDL_GPU ImGui glue is registered next to the render backend it binds
    // to; the Debugger only consumes it through IDebugImGuiBackend.
    auto& imguiRegistry = debugger::DebugImGuiBackendRegistry::GetInstance();
    if (!imguiRegistry.Contains(id))
        imguiRegistry.Register(id, [](window::IWindow& window, render::IRenderDevice& device) {
            return sdlgpu::CreateSDLGPUImGuiBackend(window, device);
        });
}

} // namespace atom::backend

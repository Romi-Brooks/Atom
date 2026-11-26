#include "RenderBackendRuntime.hpp"

#include <Atom/Backend/Extension/RenderBackendRegistry.hpp>
#include <Atom/Backend/SDLGPU/Device/SDLGPUBackend.hpp>

namespace atom::backend {

auto RenderBackendRuntime::GetInstance() -> RenderBackendRuntime& {
    static RenderBackendRuntime instance;
    return instance;
}

auto RenderBackendRuntime::EnsureDefaultRenderBackend() -> void {
    const auto id = RenderBackendRegistry::default_backend_id;
    const auto id_string = ToString(id);

    auto& registry = RenderBackendRegistry::GetInstance();
    if (!registry.ContainsBackend(id_string))
        registry.RegisterBackendFactory(id_string, [] { return std::make_unique<sdlgpu::SDLGPUBackend>(); });

}

} // namespace atom::backend

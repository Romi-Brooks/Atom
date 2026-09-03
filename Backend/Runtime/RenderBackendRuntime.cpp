#include "RenderBackendRuntime.hpp"

#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Registry/DebugImGuiBackendRegistry.hpp>
#include <Backend/Registry/RenderBackendRegistry.hpp>
#include <Backend/SDL3/Debug/SDL3DebugImGuiBackend.hpp>
#include <Backend/SDL3/Window/SDL3RenderWindow.hpp>

namespace atom::backend {

auto RenderBackendRuntime::GetInstance() -> RenderBackendRuntime& {
    static RenderBackendRuntime instance;
    return instance;
}

auto RenderBackendRuntime::EnsureDefaultRenderBackend() -> void {
    const auto& id = RenderBackendRegistry::kDefaultBackendId;

    auto& registry = RenderBackendRegistry::GetInstance();
    if (!registry.ContainsWindowBackend(id)) {
        registry.RegisterWindowFactory(id, [] { return std::make_unique<sdl3::SDL3RenderWindow>(); });
    }

    // The debug overlay (ImGui) backend belongs to the same render backend
    // bundle, so it is registered here as well (idempotent).
    auto& debugRegistry = debugger::DebugImGuiBackendRegistry::GetInstance();
    if (!debugRegistry.Contains(id)) {
        debugRegistry.Register(id, [](window::IRenderWindow& window) { return sdl3::CreateDebugImGuiBackend(window); });
    }
}

} // namespace atom::backend

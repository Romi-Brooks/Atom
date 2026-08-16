#ifndef ATOM_BACKEND_RUNTIME_RENDER_BACKEND_RUNTIME_HPP
#define ATOM_BACKEND_RUNTIME_RENDER_BACKEND_RUNTIME_HPP

namespace atom {

// Hosts the engine's built-in render backends. Mirrors BackendRuntime (audio):
// this is the sanctioned layer that knows concrete backends and registers them
// into RenderBackendRegistry; upper layers only consume the registry and the
// IRenderWindow / IRenderTarget interfaces.
class RenderBackendRuntime final {
public:
    static auto GetInstance() -> RenderBackendRuntime&;

    // Registers the engine's built-in window backends (idempotent). The
    // default is "sdl3"; a Vulkan backend would register here as well.
    auto EnsureDefaultRenderBackend() -> void;

private:
    RenderBackendRuntime() = default;
};

} // namespace atom

#endif // ATOM_BACKEND_RUNTIME_RENDER_BACKEND_RUNTIME_HPP

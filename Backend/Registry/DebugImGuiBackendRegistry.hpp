#ifndef ATOM_BACKEND_REGISTRY_DEBUG_IMGUI_BACKEND_REGISTRY_HPP
#define ATOM_BACKEND_REGISTRY_DEBUG_IMGUI_BACKEND_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace atom::window {
class IWindow;
}
namespace atom::render {
class IRenderDevice;
}

namespace atom::debugger {

class IDebugImGuiBackend;

// Registry of debug overlay (ImGui) backends, keyed by render backend id.
// Mirrors RenderBackendRegistry: backend modules register their ImGui glue
// (e.g. "sdl_gpu" -> SDL_GPU ImGui backend) at the runtime layer; the debug
// overlay layer (Debugger) only consumes IDebugImGuiBackend.
class DebugImGuiBackendRegistry final {
    public:
        using Factory = std::function<std::unique_ptr<IDebugImGuiBackend>(window::IWindow&, render::IRenderDevice&)>;

        [[nodiscard]] static auto GetInstance() -> DebugImGuiBackendRegistry&;

        auto Register(std::string_view renderBackendId, Factory factory) -> bool;

        [[nodiscard]] auto Create(std::string_view renderBackendId, window::IWindow& window,
                                  render::IRenderDevice& device) const -> std::unique_ptr<IDebugImGuiBackend>;
        [[nodiscard]] auto Contains(std::string_view renderBackendId) const -> bool;

    private:
        static auto NormalizeId(std::string_view id) -> std::string;

        std::unordered_map<std::string, Factory> debug_backends_;
};

} // namespace atom::debugger

#endif // ATOM_BACKEND_REGISTRY_DEBUG_IMGUI_BACKEND_REGISTRY_HPP

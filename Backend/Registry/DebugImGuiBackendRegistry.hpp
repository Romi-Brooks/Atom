#ifndef ATOM_BACKEND_REGISTRY_DEBUG_IMGUI_BACKEND_REGISTRY_HPP
#define ATOM_BACKEND_REGISTRY_DEBUG_IMGUI_BACKEND_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace atom {

class IRenderWindow;
class IDebugImGuiBackend;

// Registry of debug overlay (ImGui) backends, keyed by render backend id.
// Mirrors RenderBackendRegistry: backend modules register their ImGui glue
// (e.g. "sdl3" -> SDL3DebugImGuiBackend) at the runtime layer; the debug
// overlay layer (Debugger) only consumes IDebugImGuiBackend.
class DebugImGuiBackendRegistry final {
public:
    using Factory = std::function<std::unique_ptr<IDebugImGuiBackend>(IRenderWindow&)>;

    [[nodiscard]] static auto GetInstance() -> DebugImGuiBackendRegistry&;

    auto Register(std::string_view renderBackendId, Factory factory) -> bool;

    [[nodiscard]] auto Create(std::string_view renderBackendId, IRenderWindow& window) const
        -> std::unique_ptr<IDebugImGuiBackend>;
    [[nodiscard]] auto Contains(std::string_view renderBackendId) const -> bool;

private:
    static auto NormalizeId(std::string_view id) -> std::string;

    std::unordered_map<std::string, Factory> debug_backends_;
};

} // namespace atom

#endif // ATOM_BACKEND_REGISTRY_DEBUG_IMGUI_BACKEND_REGISTRY_HPP

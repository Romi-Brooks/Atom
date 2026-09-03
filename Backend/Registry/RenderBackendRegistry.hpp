#ifndef ATOM_BACKEND_REGISTRY_RENDER_BACKEND_REGISTRY_HPP
#define ATOM_BACKEND_REGISTRY_RENDER_BACKEND_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace atom::window {
class IRenderWindow;
}

namespace atom::backend {

// Registry of render backend factories (window creation). Mirrors
// BackendRegistry (audio): engine internals register concrete backends here
// (e.g. "sdl3", later "vulkan"), upper layers create windows through the
// registry instead of touching concrete backend types. Public headers must
// never include Backend/<name>/* directly.
class RenderBackendRegistry final {
    public:
        using WindowFactory = std::function<std::unique_ptr<window::IRenderWindow>()>;

        static constexpr std::string_view kDefaultBackendId = "sdl3";

        [[nodiscard]] static auto GetInstance() -> RenderBackendRegistry&;

        auto RegisterWindowFactory(std::string_view id, WindowFactory factory) -> bool;

        [[nodiscard]] auto CreateWindow(std::string_view id) const -> std::unique_ptr<window::IRenderWindow>;
        [[nodiscard]] auto ContainsWindowBackend(std::string_view id) const -> bool;

    private:
        static auto NormalizeId(std::string_view id) -> std::string;

        std::unordered_map<std::string, WindowFactory> window_backends_;
};

} // namespace atom::backend

#endif // ATOM_BACKEND_REGISTRY_RENDER_BACKEND_REGISTRY_HPP

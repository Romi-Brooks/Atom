#ifndef ATOM_BACKEND_REGISTRY_RENDER_BACKEND_REGISTRY_HPP
#define ATOM_BACKEND_REGISTRY_RENDER_BACKEND_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace atom::render {
class IRenderBackend;
}

namespace atom::backend {

// Registry of complete render backend factories. Mirrors
// BackendRegistry (audio): engine internals register concrete backends here
// (e.g. "sdl_gpu", later "vulkan"), upper layers create the window/device
// bundle through the registry instead of touching concrete backend types. Public headers must
// never include Backend/<name>/* directly.
class RenderBackendRegistry final {
    public:
        using BackendFactory = std::function<std::unique_ptr<render::IRenderBackend>()>;

        static constexpr std::string_view kDefaultBackendId = "sdl_gpu";

        [[nodiscard]] static auto GetInstance() -> RenderBackendRegistry&;

        auto RegisterBackendFactory(std::string_view id, BackendFactory factory) -> bool;

        [[nodiscard]] auto CreateBackend(std::string_view id) const -> std::unique_ptr<render::IRenderBackend>;
        [[nodiscard]] auto ContainsBackend(std::string_view id) const -> bool;

    private:
        static auto NormalizeId(std::string_view id) -> std::string;

        std::unordered_map<std::string, BackendFactory> backends_;
};

} // namespace atom::backend

#endif // ATOM_BACKEND_REGISTRY_RENDER_BACKEND_REGISTRY_HPP

#ifndef ATOM_BACKEND_REGISTRY_RENDER_BACKEND_REGISTRY_HPP
#define ATOM_BACKEND_REGISTRY_RENDER_BACKEND_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Backend/Contracts/Render/RenderBackendId.hpp>

namespace atom::render {
class IRenderBackend;
}

namespace atom::backend {

// Registry of complete render backend factories. Engine internals register
// concrete backends here; public game code selects via RenderBackendId on
// RenderWindow::Initialize. Upper layers must not include Backend/<name>/*.
class RenderBackendRegistry final {
    public:
        using BackendFactory = std::function<std::unique_ptr<render::IRenderBackend>()>;

        static constexpr RenderBackendId kDefaultBackendId = RenderBackendId::SdlGpu;

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

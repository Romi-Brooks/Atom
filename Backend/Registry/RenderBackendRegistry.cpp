#include "RenderBackendRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include <Backend/Contracts/Render/IRenderBackend.hpp>

namespace atom::backend {

auto RenderBackendRegistry::GetInstance() -> RenderBackendRegistry& {
    static RenderBackendRegistry instance;
    return instance;
}

auto RenderBackendRegistry::NormalizeId(const std::string_view id) -> std::string {
    std::string normalized{id};
    std::ranges::transform(normalized, normalized.begin(),
                           [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return normalized;
}

auto RenderBackendRegistry::RegisterBackendFactory(const std::string_view id, BackendFactory factory) -> bool {
    if (!factory)
        return false;
    return backends_.emplace(NormalizeId(id), std::move(factory)).second;
}

auto RenderBackendRegistry::CreateBackend(const std::string_view id) const -> std::unique_ptr<render::IRenderBackend> {
    const auto it = backends_.find(NormalizeId(id));
    return it == backends_.end() ? nullptr : it->second();
}

auto RenderBackendRegistry::ContainsBackend(const std::string_view id) const -> bool {
    return backends_.contains(NormalizeId(id));
}

} // namespace atom::backend

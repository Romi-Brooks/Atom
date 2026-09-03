#include "RenderBackendRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include <Backend/Contracts/Render/IRenderWindow.hpp>

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

auto RenderBackendRegistry::RegisterWindowFactory(const std::string_view id, WindowFactory factory) -> bool {
    if (!factory)
        return false;
    return window_backends_.emplace(NormalizeId(id), std::move(factory)).second;
}

auto RenderBackendRegistry::CreateWindow(const std::string_view id) const -> std::unique_ptr<window::IRenderWindow> {
    const auto it = window_backends_.find(NormalizeId(id));
    return it == window_backends_.end() ? nullptr : it->second();
}

auto RenderBackendRegistry::ContainsWindowBackend(const std::string_view id) const -> bool {
    return window_backends_.contains(NormalizeId(id));
}

} // namespace atom::backend

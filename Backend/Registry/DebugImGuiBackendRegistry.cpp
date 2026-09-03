#include "DebugImGuiBackendRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>

namespace atom::debugger {

auto DebugImGuiBackendRegistry::GetInstance() -> DebugImGuiBackendRegistry& {
    static DebugImGuiBackendRegistry instance;
    return instance;
}

auto DebugImGuiBackendRegistry::NormalizeId(const std::string_view id) -> std::string {
    std::string normalized{id};
    std::ranges::transform(normalized, normalized.begin(),
                           [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return normalized;
}

auto DebugImGuiBackendRegistry::Register(const std::string_view renderBackendId, Factory factory) -> bool {
    if (!factory)
        return false;
    return debug_backends_.emplace(NormalizeId(renderBackendId), std::move(factory)).second;
}

auto DebugImGuiBackendRegistry::Create(const std::string_view renderBackendId, window::IWindow& window,
                                       render::IRenderDevice& device) const -> std::unique_ptr<IDebugImGuiBackend> {
    const auto it = debug_backends_.find(NormalizeId(renderBackendId));
    return it == debug_backends_.end() ? nullptr : it->second(window, device);
}

auto DebugImGuiBackendRegistry::Contains(const std::string_view renderBackendId) const -> bool {
    return debug_backends_.contains(NormalizeId(renderBackendId));
}

} // namespace atom::debugger

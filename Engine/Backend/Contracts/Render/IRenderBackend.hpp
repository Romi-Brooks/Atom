/**
  * @file           : IRenderBackend.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral render backend lifecycle contract.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_IRENDERBACKEND_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_IRENDERBACKEND_HPP

#include <string>

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Render/RenderBackendConfig.hpp>

namespace atom::render {
class IRenderDevice;
}
namespace atom::window {
class IWindow;
}
namespace atom::backend {
class ITimeSource;
}

namespace atom::render {

class IRenderBackend {
    public:
        virtual ~IRenderBackend() = default;
        // Bring the backend up from one request struct. Implementations apply
        // the window placement before claiming the window for presentation (a
        // fullscreen transition recreates the swapchain, so doing it first
        // avoids tearing down a live one), then apply the present mode.
        //
        // A backend that cannot honor config.driver or config.present_mode
        // must log a warning and fall back, not fail: an unstartable engine in
        // a shipping game is worse than a driver the player did not pick.
        virtual auto Initialize(const backend::RenderBackendConfig& config) -> bool = 0;
        virtual auto Shutdown() -> void = 0;
        [[nodiscard]] virtual auto Window() -> window::IWindow& = 0;
        [[nodiscard]] virtual auto Device() -> IRenderDevice& = 0;
        // High-resolution host clock for atom::time. Bound to the backend's
        // native counter (SDL_GetPerformanceCounter on SDL3).
        [[nodiscard]] virtual auto TimeSource() -> backend::ITimeSource& = 0;
};

} // namespace atom::render

#endif // ATOM_BACKEND_CONTRACTS_RENDER_IRENDERBACKEND_HPP

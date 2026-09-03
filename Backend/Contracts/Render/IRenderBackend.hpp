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

namespace atom::render {
class IRenderDevice;
}
namespace atom::window {
class IWindow;
}

namespace atom::render {

class IRenderBackend {
    public:
        virtual ~IRenderBackend() = default;
        virtual auto Initialize(const std::string& title, algo::Vec2 resolution) -> bool = 0;
        virtual auto Shutdown() -> void = 0;
        [[nodiscard]] virtual auto Window() -> window::IWindow& = 0;
        [[nodiscard]] virtual auto Device() -> IRenderDevice& = 0;
};

} // namespace atom::render

#endif

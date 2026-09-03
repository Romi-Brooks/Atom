/**
  * @file           : IRenderDevice.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral render device capability contract.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_IRENDERDEVICE_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_IRENDERDEVICE_HPP

#include <cstdint>
#include <string>

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Render/RenderTypes.hpp>

namespace atom::render {

struct RenderBackendInfo {
        std::string api;
        std::string device;
        std::string driver;
        uint32_t shader_formats = 0;
        uint32_t swapchain_format = 0;
};

class IRenderDevice {
    public:
        virtual ~IRenderDevice() = default;
        [[nodiscard]] virtual auto BeginFrame() -> bool = 0;
        virtual auto Clear(const Color& color = Color::Black()) -> void = 0;
        virtual auto EndFrame() -> void = 0;
        virtual auto HandleResize(uint32_t width, uint32_t height) -> void = 0;
        virtual auto SetVSync(bool enabled) -> bool = 0;
        [[nodiscard]] virtual auto IsVSyncEnabled() const -> bool = 0;
        [[nodiscard]] virtual auto GetOutputSize() const -> algo::Vec2 = 0;
        [[nodiscard]] virtual auto GetBackendInfo() const -> const RenderBackendInfo& = 0;
};

} // namespace atom::render

#endif

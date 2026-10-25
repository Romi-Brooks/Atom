/**
  * @file           : RenderBackendConfig.hpp
  * @author         : Romi Brooks
  * @brief          : Startup request for a render backend: backend + driver +
  *                   window placement + present mode.
  * @attention      : Two independent axes. Do not fold them into one enum.
  * @date           : 2026/10/23
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_RENDERBACKENDCONFIG_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_RENDERBACKENDCONFIG_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Render/PresentMode.hpp>
#include <Backend/Contracts/Render/RenderBackendId.hpp>
#include <Backend/Contracts/Window/WindowMode.hpp>

namespace atom::backend {

// Which graphics API a backend talks to. This is NOT a backend id.
//
//   RenderBackendId  selects the *implementation* of IRenderBackend: the code
//                    that owns the device, the swapchain, the pipelines and
//                    the resource caches. There is one per productized
//                    backend, and a new one needs a new enum member.
//   GpuDriver        selects the *API* underneath a backend that can target
//                    more than one. SDL_GPU is exactly such a backend: the
//                    same code runs on D3D12, Vulkan or Metal.
//
// So "SDL_GPU over Vulkan" is RenderBackendId::SdlGpu + GpuDriver::Vulkan, and
// a future engine-owned Vulkan implementation would be
// RenderBackendId::Vulkan + GpuDriver::Auto. Combining the two into one enum
// would multiply members by drivers and stop answering "which code implements
// this backend?".
enum class GpuDriver : std::uint8_t {
    // Let the backend pick (SDL_GPU prefers D3D12 on Windows, Metal on macOS,
    // Vulkan elsewhere). This is what a shipping game should ask for.
    Auto = 0,
    Direct3D12,
    Vulkan,
    Metal,
};

[[nodiscard]] constexpr auto ToString(const GpuDriver driver) -> std::string_view {
    switch (driver) {
    case GpuDriver::Auto:
        return "auto";
    case GpuDriver::Direct3D12:
        return "direct3d12";
    case GpuDriver::Vulkan:
        return "vulkan";
    case GpuDriver::Metal:
        return "metal";
    }
    return "auto";
}

// Whether a backend should enable its API's debug/validation layer.
//
//   Auto  Follow the build type: on when NDEBUG is absent. This is what both a
//         developer and a shipped build want, and it is the default.
//   Off   Force it off. Profiling runs need this: validation turns a
//         draw-call-heavy frame from fractions of a millisecond into several,
//         which makes any frame-rate measurement from a Debug binary meaningless.
//   On    Force it on, e.g. to validate a Release build of a backend change.
//
// Three states rather than a bool because "false" cannot express "do what the
// build type says", and hard-coding that at every call site is how a profiling
// build quietly keeps validation on.
enum class ValidationMode : std::uint8_t {
    Auto = 0,
    Off,
    On,
};

[[nodiscard]] constexpr auto ToString(const ValidationMode mode) -> std::string_view {
    switch (mode) {
    case ValidationMode::Auto:
        return "auto";
    case ValidationMode::Off:
        return "off";
    case ValidationMode::On:
        return "on";
    }
    return "auto";
}

// Everything a backend needs to come up. One struct rather than a growing
// positional-argument list, so adding an option never breaks call sites.
//
// Defaults describe the conservative desktop case: windowed, vertical sync on,
// backend-chosen driver. Game code opts into anything else explicitly.
struct RenderBackendConfig {
        std::string title = "Atom";
        algo::Vec2 resolution{1280.0f, 720.0f};
        // Which IRenderBackend implementation to instantiate.
        RenderBackendId backend = RenderBackendId::SdlGpu;
        // Which API the backend should run on, when it has a choice.
        GpuDriver driver = GpuDriver::Auto;
        // Whether to enable the API's debug/validation layer.
        ValidationMode validation = ValidationMode::Auto;
        // How the window occupies the display.
        window::WindowMode window_mode = window::WindowMode::Windowed;
        // How frames reach the display. VSync is the only mode every backend
        // and every driver is required to support, so it is the default; a
        // requested mode the backend cannot honor must be reported (the backend
        // logs a warning and keeps VSync) rather than silently substituted.
        render::PresentMode present_mode = render::PresentMode::VSync;
};

} // namespace atom::backend

#endif // ATOM_BACKEND_CONTRACTS_RENDER_RENDERBACKENDCONFIG_HPP

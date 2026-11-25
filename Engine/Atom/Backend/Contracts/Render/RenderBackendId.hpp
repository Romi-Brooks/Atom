// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file RenderBackendId.hpp
 * @brief Public render-backend selection tokens (engine-curated, not open to mods).
 */

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_RENDER_BACKEND_ID_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_RENDER_BACKEND_ID_HPP

#include <string_view>

namespace atom::backend {

// Closed set of render backends shipped with Atom. Game code selects with this
// enum; engine developers add members when a new backend is productized.
// Custom runtime factories remain an internal Extension concern.
enum class RenderBackendId {
    SdlGpu,
    // Vulkan,  // reserved for a future native backend
};

[[nodiscard]] constexpr auto ToString(const RenderBackendId id) -> std::string_view {
    switch (id) {
    case RenderBackendId::SdlGpu:
        return "sdl_gpu";
    }
    return "sdl_gpu";
}

} // namespace atom::backend

#endif // ATOM_BACKEND_CONTRACTS_RENDER_RENDER_BACKEND_ID_HPP

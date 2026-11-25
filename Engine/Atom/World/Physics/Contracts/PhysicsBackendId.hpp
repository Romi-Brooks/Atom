// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file PhysicsBackendId.hpp
 * @brief 引擎内置物理后端的选择令牌（对齐 AudioBackendId）。
 */

#ifndef ATOM_PHYSICS_BACKEND_PHYSICS_BACKEND_ID_HPP
#define ATOM_PHYSICS_BACKEND_PHYSICS_BACKEND_ID_HPP

#include <string_view>

namespace atom::backend {

// 引擎内置物理后端的封闭集合。Box2D 等外部库后端在接入时追加。
enum class PhysicsBackendId {
        Internal,
};

[[nodiscard]] constexpr auto ToString(const PhysicsBackendId id) -> std::string_view {
    switch (id) {
    case PhysicsBackendId::Internal:
        return "internal";
    }
    return "internal";
}

} // namespace atom::backend

#endif // ATOM_PHYSICS_BACKEND_PHYSICS_BACKEND_ID_HPP

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Contact.hpp
 * @brief 接触点与接触流形（世界坐标）。
 *
 * 法线约定：manifold.normal 指向形状 A（从 B 指向 A）。求解时 A 沿 +normal
 * 分离、B 沿 -normal 分离。接触点最多 2 个（面-面接触）。
 */

#ifndef ATOM_PHYSICS_COLLISION_CONTACT_HPP
#define ATOM_PHYSICS_COLLISION_CONTACT_HPP

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::physics {

struct ContactPoint final {
        algo::Vec2 point{};        // 接触点世界坐标
        float penetration = 0.0f;  // 穿透深度（正数）
};

struct ContactManifold final {
        algo::Vec2 normal{}; // 单位法线，指向形状 A
        ContactPoint points[2]{};
        int point_count = 0;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_COLLISION_CONTACT_HPP

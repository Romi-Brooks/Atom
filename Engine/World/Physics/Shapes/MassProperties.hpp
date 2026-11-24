// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file MassProperties.hpp
 * @brief 由形状 + 密度计算出的质量属性。
 *
 * 所有量都在形状的局部空间：centroid 是相对局部原点的质心偏移；inertia 是
 * 绕「质心」的转动惯量（不是绕局部原点）。刚体做旋转动力学时两者都要用。
 */

#ifndef ATOM_PHYSICS_SHAPES_MASS_PROPERTIES_HPP
#define ATOM_PHYSICS_SHAPES_MASS_PROPERTIES_HPP

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::physics {

struct MassProperties final {
        float mass = 0.0f;            // 质量（= 面积 × 密度）
        float inverse_mass = 0.0f;    // 1/mass；质量为 0（静态/退化）时为 0
        float inertia = 0.0f;         // 绕质心的转动惯量
        float inverse_inertia = 0.0f; // 1/inertia；惯量为 0 时为 0
        algo::Vec2 centroid{};        // 质心相对局部原点的偏移
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_SHAPES_MASS_PROPERTIES_HPP

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Material.hpp
 * @brief 形状的表面/材质属性。随形状一起定义（BodyDef 中 shape + material）。
 *
 * 材质是纯数据，不进求解器公式之外的任何行为：restitution 决定弹性恢复、
 * friction 决定库仑摩擦、density 决定质量（质量 = 面积 × 密度）。
 */

#ifndef ATOM_PHYSICS_SHAPES_MATERIAL_HPP
#define ATOM_PHYSICS_SHAPES_MATERIAL_HPP

namespace atom::physics {

struct Material final {
        // 弹性恢复系数，通常 [0, 1]。0 = 完全非弹性，1 = 完全弹性。
        float restitution = 0.0f;
        // 库仑摩擦系数，通常 [0, 1]。
        float friction = 0.2f;
        // 密度（质量 / 面积）。质量 = 形状面积 × 密度。
        float density = 1.0f;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_SHAPES_MATERIAL_HPP

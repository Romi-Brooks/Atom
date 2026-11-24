// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file NarrowPhase.hpp
 * @brief 窄相位：两个形状的精确碰撞检测与接触流形生成。
 *
 * 纯函数，不依赖后端。支持圆与凸多边形；Segment 的地形碰撞留到后续里程碑。
 * 形状是数据、碰撞是函数——新增形状类型时在这里加一对组合即可，不改求解器。
 */

#ifndef ATOM_PHYSICS_COLLISION_NARROW_PHASE_HPP
#define ATOM_PHYSICS_COLLISION_NARROW_PHASE_HPP

#include <Algorithm/Vector/Vec2.hpp>
#include <Physics/Collision/Contact.hpp>
#include <Physics/Shapes/Shape.hpp>

namespace atom::physics {

// 检测两个形状（世界 transform）是否碰撞。相交返回 true 并填充 manifold
// （normal 指向 a）。圆/凸多边形组合支持；含 Segment 时返回 false。
[[nodiscard]] auto Collide(const Shape& a, algo::Vec2 position_a, float angle_a, const Shape& b, algo::Vec2 position_b,
                           float angle_b, ContactManifold& manifold) -> bool;

} // namespace atom::physics

#endif // ATOM_PHYSICS_COLLISION_NARROW_PHASE_HPP

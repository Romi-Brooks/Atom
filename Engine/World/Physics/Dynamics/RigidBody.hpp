// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file RigidBody.hpp
 * @brief 刚体动力学状态 + 半隐式积分（纯数据 + 纯函数，不依赖后端）。
 *
 * RigidBody 是物理内部的紧凑状态；契约（IPhysicsBody）由 Internal 后端包装。
 * 积分用半隐式（symplectic）Euler：先速度后位置，能量性质优于显式 Euler，
 * 是后续接触求解器（Sequential Impulse）的正确前置。
 */

#ifndef ATOM_PHYSICS_DYNAMICS_RIGID_BODY_HPP
#define ATOM_PHYSICS_DYNAMICS_RIGID_BODY_HPP

#include <Algorithm/Geometry/Primitives.hpp>
#include <Algorithm/Vector/Vec2.hpp>
#include <Physics/Contracts/PhysicsTypes.hpp>
#include <Physics/Shapes/Material.hpp>
#include <Physics/Shapes/Shape.hpp>

namespace atom::physics {

struct RigidBody final {
        BodyType type = BodyType::Static;
        Shape shape{};        // 局部空间形状
        Material material{};

        // 世界空间状态（position = 刚体原点；质心世界位置 = position + local_center.Rotated(angle)）
        algo::Vec2 position{};
        float angle = 0.0f;
        algo::Vec2 local_center{}; // 形状质心的局部偏移（求解器以质心为旋转参考）
        algo::Vec2 linear_velocity{};
        float angular_velocity = 0.0f;

        // 质量属性（inv 形式，便于积分与求解）
        float mass = 0.0f;
        float inverse_mass = 0.0f;
        float inertia = 0.0f;
        float inverse_inertia = 0.0f;

        // 本步累积的力/力矩（Integrate 之后清零）
        algo::Vec2 force{};
        float torque = 0.0f;

        // 阻尼（0 = 无阻尼）
        float linear_damping = 0.0f;
        float angular_damping = 0.0f;

        // 休眠状态（仅 Dynamic 参与；由 InternalPhysicsWorld 的睡眠策略维护）
        float sleep_timer = 0.0f; // 连续低于速度阈值的累计秒数
        bool asleep = false;

        // 世界空间 AABB（宽相位代理，由 UpdateWorldAabb 维护）
        algo::Aabb2 aabb{};
};

// 半隐式 Euler 拆成两步：求解器在速度积分与位置积分之间插入碰撞求解。
// IntegrateVelocity 消费重力与累积力并清零；IntegratePosition 推进位置。
auto IntegrateVelocity(RigidBody& body, float dt, algo::Vec2 gravity) -> void;
auto IntegratePosition(RigidBody& body, float dt) -> void;
// 便捷合并版（无碰撞场景用），等价于先 velocity 后 position。
auto Integrate(RigidBody& body, float dt, algo::Vec2 gravity) -> void;

// 由局部形状 + 世界 transform 计算世界 AABB，写回 body.aabb。
auto UpdateWorldAabb(RigidBody& body) -> void;

// 纯函数版：不修改 body。
[[nodiscard]] auto ComputeWorldAabb(const Shape& shape, algo::Vec2 position, float angle) -> algo::Aabb2;

} // namespace atom::physics

#endif // ATOM_PHYSICS_DYNAMICS_RIGID_BODY_HPP

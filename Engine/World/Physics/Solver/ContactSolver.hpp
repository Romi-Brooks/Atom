// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file ContactSolver.hpp
 * @brief Sequential Impulse 接触求解器。
 *
 * 物理系统的数值核心。对每个接触流形建立约束，迭代求解：
 *   速度求解（法向 restitution + 切向摩擦，冲量迭代，保证法向冲量非负）；
 *   位置求解（重算穿透后按比例把刚体推离，消除残余穿透）。
 * 拆成两个独立调用点，因为主循环要在「速度求解」与「位置求解」之间积分位置。
 *
 * 不跨帧持久化接触（无 warm starting）：每步从 0 冲量开始，靠位置求解保证
 * 堆叠稳定。warm starting 是后续优化（需 contact id 追踪与跨帧缓存）。
 */

#ifndef ATOM_PHYSICS_SOLVER_CONTACT_SOLVER_HPP
#define ATOM_PHYSICS_SOLVER_CONTACT_SOLVER_HPP

#include <vector>

#include <Algorithm/Vector/Vec2.hpp>
#include <Physics/Collision/Contact.hpp>

namespace atom::physics {

struct RigidBody;

struct ContactSolverConfig final {
        float velocity_threshold = 1.0f; // 相对法向速度超过它才启用 restitution（米/秒）
        float position_slop = 0.005f;    // 允许的残余穿透（米）
        float baumgarte = 0.2f;          // 位置修正比例 [0, 1]
};

struct ContactSolverInput final {
        RigidBody* a = nullptr;
        RigidBody* b = nullptr;
        ContactManifold manifold{};
};

// 速度求解：Prepare + 迭代 velocity_iterations 次。在「积分速度」之后、「积分位置」之前调用。
auto SolveVelocityContacts(std::vector<ContactSolverInput>& contacts, int velocity_iterations,
                           const ContactSolverConfig& config) -> void;

// 位置求解：重算当前穿透并迭代修正。在「积分位置」之后调用。
auto SolvePositionContacts(std::vector<ContactSolverInput>& contacts, int position_iterations,
                           const ContactSolverConfig& config) -> void;

} // namespace atom::physics

#endif // ATOM_PHYSICS_SOLVER_CONTACT_SOLVER_HPP

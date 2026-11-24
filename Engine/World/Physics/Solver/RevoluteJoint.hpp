// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file RevoluteJoint.hpp
 * @brief revolute 关节（铰链）约束：消除两刚体锚点间的相对平动，保留相对旋转。
 *
 * 2D 点约束（point-to-point），等价于 Box2D b2RevoluteJoint 去掉 motor/limit 的核心。
 * 约束以质心为旋转参照（与接触求解器一致）；求解按 2×2 有效质量矩阵（对称）逐点消去
 * 相对速度 / 位置误差。本里程碑作为「新增约束类型不改框架」的证明——未来 distance、
 * prismatic、weld 等关节按同一模式追加即可。
 */

#ifndef ATOM_PHYSICS_SOLVER_REVOLUTE_JOINT_HPP
#define ATOM_PHYSICS_SOLVER_REVOLUTE_JOINT_HPP

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::physics {

struct RigidBody;

struct RevoluteJoint final {
        RigidBody* a = nullptr;
        RigidBody* b = nullptr;
        algo::Vec2 local_anchor_a{};
        algo::Vec2 local_anchor_b{};

        // 每步缓存（Prepare 按当前 transform 重算）。
        algo::Vec2 r_a{}; // worldAnchorA − centroidA
        algo::Vec2 r_b{};
        float k11 = 0.0f;
        float k12 = 0.0f;
        float k22 = 0.0f;

        // 依据当前 transform 计算锚点/质心杠杆臂与有效质量矩阵 K。
        auto Prepare() -> void;
        // 速度约束：单次求解（无 warm starting，够用且保持确定）。
        auto SolveVelocity() -> void;
        // 位置约束：把锚点误差线性投影回一致（NGS 风格单次修正）。
        auto SolvePosition() -> void;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_SOLVER_REVOLUTE_JOINT_HPP

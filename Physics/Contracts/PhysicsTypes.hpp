// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file PhysicsTypes.hpp
 * @brief 物理后端的公共值类型：刚体类型与刚体定义。
 *
 * 单位约定（见 Docs/Physics-Plan-CN.md）：物理世界内部用米制 + y 向上。
 * 渲染层（像素 + y 向下）与物理层之间由游戏侧通过 PPM（每米像素数）换算。
 */

#ifndef ATOM_PHYSICS_BACKEND_PHYSICS_TYPES_HPP
#define ATOM_PHYSICS_BACKEND_PHYSICS_TYPES_HPP

#include <cstdint>

#include <Algorithm/Vector/Vec2.hpp>
#include <Physics/Shapes/Material.hpp>
#include <Physics/Shapes/Shape.hpp>

namespace atom::physics {

enum class BodyType : std::uint8_t {
        Static,    // 不参与动力学求解，但参与碰撞（地形、墙体）
        Kinematic, // 由代码驱动运动，不响应力，但会把速度传给与之接触的动态体
        Dynamic,   // 完全参与动力学（受力、积分、碰撞响应）
};

// 创建刚体时的描述。复合形状（多 fixture）、阻尼、重力缩放、固定旋转仍是后续扩展点；
// sensor 与 user_data 已在 T3.1 落地（见下）。
struct BodyDef final {
        Shape shape{};
        Material material{};
        BodyType type = BodyType::Static;
        algo::Vec2 position{}; // 世界坐标（米）
        float angle = 0.0f;    // 弧度

        // 触发器：参与碰撞检测并上报接触，但不接受任何求解响应。这是"玩家在触发区里"
        // 与"触发区把玩家推开"的区别，也是终点 / 伤害区 / 检测区能实现的前提。
        bool is_sensor = false;

        // 拥有者身份（ECS 的 backend handle、场景 id 或任何宿主编号）。用数字而不是
        // void*：契约里放裸指针等于邀请"拥有者已销毁后仍被读"，而集成层只需要反查。
        // 创建后不可变——需要换身份就重建刚体，避免生命周期中途换主。
        std::uint64_t user_data = 0;
};

// revolute 关节定义：把两个刚体在各自局部空间的锚点钉到同一个世界点。
// 约束只消除相对平动，保留相对旋转（铰链）。motor/角度限制留作扩展点。
struct RevoluteJointDef final {
        algo::Vec2 local_anchor_a{}; // 在刚体 A 局部空间（相对 body origin）
        algo::Vec2 local_anchor_b{}; // 在刚体 B 局部空间
        bool collide_connected = true; // false 时两刚体之间不产生碰撞
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_PHYSICS_TYPES_HPP

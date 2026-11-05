// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file IPhysicsJoint.hpp
 * @brief 关节契约。游戏侧只依赖此接口，不接触具体后端实现。
 *
 * 关节连接两个刚体，约束其相对运动（revolute = 铰链，只消除相对平动）。
 * 公开头不出现任何第三方类型。
 */

#ifndef ATOM_PHYSICS_BACKEND_I_PHYSICS_JOINT_HPP
#define ATOM_PHYSICS_BACKEND_I_PHYSICS_JOINT_HPP

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::physics {

class IPhysicsBody;

class IPhysicsJoint {
    public:
        virtual ~IPhysicsJoint() = default;

        [[nodiscard]] virtual auto GetBodyA() const -> IPhysicsBody* = 0;
        [[nodiscard]] virtual auto GetBodyB() const -> IPhysicsBody* = 0;

        // 锚点的当前世界坐标（米）。
        [[nodiscard]] virtual auto GetAnchorA() const -> algo::Vec2 = 0;
        [[nodiscard]] virtual auto GetAnchorB() const -> algo::Vec2 = 0;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_I_PHYSICS_JOINT_HPP

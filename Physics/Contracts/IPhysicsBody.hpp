// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file IPhysicsBody.hpp
 * @brief 刚体契约。游戏侧只依赖此接口，不接触具体后端实现。
 *
 * 世界坐标用米制 + y 向上；角度为弧度。公开头不出现任何第三方类型。
 */

#ifndef ATOM_PHYSICS_BACKEND_I_PHYSICS_BODY_HPP
#define ATOM_PHYSICS_BACKEND_I_PHYSICS_BODY_HPP

#include <Algorithm/Vector/Vec2.hpp>
#include <Physics/Contracts/PhysicsTypes.hpp>

namespace atom::physics {

class IPhysicsBody {
    public:
        virtual ~IPhysicsBody() = default;

        [[nodiscard]] virtual auto GetType() const -> BodyType = 0;

        [[nodiscard]] virtual auto GetPosition() const -> algo::Vec2 = 0;
        [[nodiscard]] virtual auto GetAngle() const -> float = 0;
        virtual auto SetTransform(algo::Vec2 position, float angle) -> void = 0;

        [[nodiscard]] virtual auto GetLinearVelocity() const -> algo::Vec2 = 0;
        virtual auto SetLinearVelocity(algo::Vec2 velocity) -> void = 0;
        [[nodiscard]] virtual auto GetAngularVelocity() const -> float = 0;
        virtual auto SetAngularVelocity(float angular_velocity) -> void = 0;

        // 在给定世界点施加力/冲量（会自动产生相应的力矩）。
        // 这些修改操作都会唤醒休眠中的刚体。
        virtual auto ApplyForce(algo::Vec2 force, algo::Vec2 world_point) -> void = 0;
        virtual auto ApplyLinearImpulse(algo::Vec2 impulse, algo::Vec2 world_point) -> void = 0;
        virtual auto ApplyTorque(float torque) -> void = 0;

        // 休眠查询/强制唤醒或入睡。休眠体不参与积分与求解，直到被唤醒。
        [[nodiscard]] virtual auto IsAwake() const -> bool = 0;
        virtual auto SetAwake(bool awake) -> void = 0;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_I_PHYSICS_BODY_HPP

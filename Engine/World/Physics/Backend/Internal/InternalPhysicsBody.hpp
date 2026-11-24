// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file InternalPhysicsBody.hpp
 * @brief Internal 后端的刚体：包装 RigidBody，把 IPhysicsBody 契约转发到内部状态。
 */

#ifndef ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_BODY_HPP
#define ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_BODY_HPP

#include <cstdint>

#include <Physics/Contracts/IPhysicsBody.hpp>
#include <Physics/Dynamics/RigidBody.hpp>

namespace atom::physics {

class InternalPhysicsBody final : public IPhysicsBody {
    public:
        // sensor / user_data 在创建时固定：它们描述"这个刚体属于谁、必须怎么表现"，
        // 中途改写等于给自己埋一个生命周期 bug。需要换身份就重建刚体。
        InternalPhysicsBody(RigidBody body, bool is_sensor, std::uint64_t user_data);

        [[nodiscard]] auto GetType() const -> BodyType override;
        [[nodiscard]] auto GetPosition() const -> algo::Vec2 override;
        [[nodiscard]] auto GetAngle() const -> float override;
        auto SetTransform(algo::Vec2 position, float angle) -> void override;
        [[nodiscard]] auto GetLinearVelocity() const -> algo::Vec2 override;
        auto SetLinearVelocity(algo::Vec2 velocity) -> void override;
        [[nodiscard]] auto GetAngularVelocity() const -> float override;
        auto SetAngularVelocity(float angular_velocity) -> void override;
        auto ApplyForce(algo::Vec2 force, algo::Vec2 world_point) -> void override;
        auto ApplyLinearImpulse(algo::Vec2 impulse, algo::Vec2 world_point) -> void override;
        auto ApplyTorque(float torque) -> void override;
        [[nodiscard]] auto IsAwake() const -> bool override;
        auto SetAwake(bool awake) -> void override;
        [[nodiscard]] auto IsSensor() const -> bool override;
        [[nodiscard]] auto GetUserData() const -> std::uint64_t override;

        [[nodiscard]] auto Body() -> RigidBody& {
            return body_;
        }
        [[nodiscard]] auto Body() const -> const RigidBody& {
            return body_;
        }

    private:
        // 内部唤醒辅助（清 asleep + sleep_timer）。公开 SetAwake/修改器走这里。
        auto Wake() -> void;

        RigidBody body_{};
        bool is_sensor_ = false;
        std::uint64_t user_data_ = 0;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_BODY_HPP

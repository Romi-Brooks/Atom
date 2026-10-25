// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Backend/Internal/InternalPhysicsBody.hpp>

#include <utility>
#include <cmath>

namespace atom::physics {

InternalPhysicsBody::InternalPhysicsBody(RigidBody body) : body_(std::move(body)) {}

auto InternalPhysicsBody::GetType() const -> BodyType {
    return body_.type;
}

auto InternalPhysicsBody::GetPosition() const -> algo::Vec2 {
    return body_.position;
}

auto InternalPhysicsBody::GetAngle() const -> float {
    return body_.angle;
}

auto InternalPhysicsBody::SetTransform(const algo::Vec2 position, const float angle) -> void {
    if (!std::isfinite(position.GetX()) || !std::isfinite(position.GetY()) || !std::isfinite(angle)) {
        return;
    }
    body_.position = position;
    body_.angle = angle;
    Wake();
}

auto InternalPhysicsBody::GetLinearVelocity() const -> algo::Vec2 {
    return body_.linear_velocity;
}

auto InternalPhysicsBody::SetLinearVelocity(const algo::Vec2 velocity) -> void {
    if (!std::isfinite(velocity.GetX()) || !std::isfinite(velocity.GetY())) {
        return;
    }
    body_.linear_velocity = velocity;
    Wake();
}

auto InternalPhysicsBody::GetAngularVelocity() const -> float {
    return body_.angular_velocity;
}

auto InternalPhysicsBody::SetAngularVelocity(const float angular_velocity) -> void {
    if (!std::isfinite(angular_velocity)) {
        return;
    }
    body_.angular_velocity = angular_velocity;
    Wake();
}

auto InternalPhysicsBody::ApplyForce(const algo::Vec2 force, const algo::Vec2 world_point) -> void {
    if (body_.type != BodyType::Dynamic || !std::isfinite(force.GetX()) || !std::isfinite(force.GetY()) ||
        !std::isfinite(world_point.GetX()) || !std::isfinite(world_point.GetY())) {
        return;
    }
    body_.force += force;
    body_.torque += (world_point - body_.position).Cross(force);
    Wake();
}

auto InternalPhysicsBody::ApplyLinearImpulse(const algo::Vec2 impulse, const algo::Vec2 world_point) -> void {
    if (body_.type != BodyType::Dynamic || !std::isfinite(impulse.GetX()) || !std::isfinite(impulse.GetY()) ||
        !std::isfinite(world_point.GetX()) || !std::isfinite(world_point.GetY())) {
        return;
    }
    body_.linear_velocity += impulse * body_.inverse_mass;
    body_.angular_velocity += body_.inverse_inertia * (world_point - body_.position).Cross(impulse);
    Wake();
}

auto InternalPhysicsBody::ApplyTorque(const float torque) -> void {
    if (body_.type != BodyType::Dynamic || !std::isfinite(torque)) {
        return;
    }
    body_.torque += torque;
    Wake();
}

auto InternalPhysicsBody::IsAwake() const -> bool {
    return !body_.asleep;
}

auto InternalPhysicsBody::SetAwake(const bool awake) -> void {
    if (awake) {
        Wake();
    } else {
        // 强制入睡：清零速度，避免带速休眠导致下一帧瞬移。
        body_.asleep = true;
        body_.sleep_timer = 0.0f;
        body_.linear_velocity = algo::Vec2::Zero();
        body_.angular_velocity = 0.0f;
    }
}

auto InternalPhysicsBody::Wake() -> void {
    body_.asleep = false;
    body_.sleep_timer = 0.0f;
}

} // namespace atom::physics

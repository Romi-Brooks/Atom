// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Dynamics/RigidBody.hpp>

#include <algorithm>
#include <cstddef>

namespace atom::physics {

auto IntegrateVelocity(RigidBody& body, const float dt, const algo::Vec2 gravity) -> void {
    switch (body.type) {
    case BodyType::Static:
        body.linear_velocity = algo::Vec2::Zero();
        body.angular_velocity = 0.0f;
        break;

    case BodyType::Kinematic:
        // 速度由代码驱动，不受力 / 重力 / 阻尼。
        break;

    case BodyType::Dynamic:
        // 速度先积分：v += (g + F/m)·dt，再施加速度阻尼。
        body.linear_velocity += (gravity + body.force * body.inverse_mass) * dt;
        body.linear_velocity *= 1.0f / (1.0f + dt * body.linear_damping);
        body.angular_velocity += body.torque * body.inverse_inertia * dt;
        body.angular_velocity *= 1.0f / (1.0f + dt * body.angular_damping);
        break;
    }

    body.force = algo::Vec2::Zero();
    body.torque = 0.0f;
}

auto IntegratePosition(RigidBody& body, const float dt) -> void {
    switch (body.type) {
    case BodyType::Static:
        break;
    case BodyType::Kinematic:
    case BodyType::Dynamic:
        body.position += body.linear_velocity * dt;
        body.angle += body.angular_velocity * dt;
        break;
    }
}

auto Integrate(RigidBody& body, const float dt, const algo::Vec2 gravity) -> void {
    IntegrateVelocity(body, dt, gravity);
    IntegratePosition(body, dt);
}

auto UpdateWorldAabb(RigidBody& body) -> void {
    body.aabb = ComputeWorldAabb(body.shape, body.position, body.angle);
}

auto ComputeWorldAabb(const Shape& shape, const algo::Vec2 position, const float angle) -> algo::Aabb2 {
    switch (shape.GetKind()) {
    case Shape::Kind::Circle: {
        const auto& c = shape.GetCircle();
        const algo::Vec2 center = position + c.center.Rotated(angle);
        const algo::Vec2 r{c.radius, c.radius};
        return algo::Aabb2{center - r, center + r};
    }
    case Shape::Kind::Polygon: {
        const auto& p = shape.GetPolygon();
        algo::Vec2 min = position + p.vertices[0].Rotated(angle);
        algo::Vec2 max = min;
        for (std::size_t i = 1; i < p.count; ++i) {
            const algo::Vec2 w = position + p.vertices[i].Rotated(angle);
            min = algo::Vec2{std::min(min.GetX(), w.GetX()), std::min(min.GetY(), w.GetY())};
            max = algo::Vec2{std::max(max.GetX(), w.GetX()), std::max(max.GetY(), w.GetY())};
        }
        return algo::Aabb2{min, max};
    }
    case Shape::Kind::Segment: {
        const auto& s = shape.GetSegment();
        const algo::Vec2 wa = position + s.a.Rotated(angle);
        const algo::Vec2 wb = position + s.b.Rotated(angle);
        return algo::Aabb2{algo::Vec2{std::min(wa.GetX(), wb.GetX()), std::min(wa.GetY(), wb.GetY())},
                           algo::Vec2{std::max(wa.GetX(), wb.GetX()), std::max(wa.GetY(), wb.GetY())}};
    }
    }
    return algo::Aabb2{};
}

} // namespace atom::physics

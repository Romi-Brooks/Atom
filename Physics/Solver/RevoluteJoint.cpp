// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Solver/RevoluteJoint.hpp>

#include <cmath>

#include <Physics/Dynamics/RigidBody.hpp>

namespace atom::physics {

namespace {

// 标量叉乘向量：ω × r = (-ω·r.y, ω·r.x)。
[[nodiscard]] inline auto CrossScalarVec(const float s, const algo::Vec2& v) -> algo::Vec2 {
    return algo::Vec2{-s * v.GetY(), s * v.GetX()};
}

// 质心世界位置（与接触求解器一致：position + local_center.Rotated(angle)）。
[[nodiscard]] inline auto Centroid(const RigidBody& body) -> algo::Vec2 {
    return body.position + body.local_center.Rotated(body.angle);
}

// 求解对称 2×2 线性方程组 K·x = rhs，返回 x；K 奇异时返回零向量。
[[nodiscard]] inline auto Solve2x2(const float k11, const float k12, const float k22, const algo::Vec2& rhs)
    -> algo::Vec2 {
    const float det = k11 * k22 - k12 * k12;
    if (std::abs(det) < 1.0e-12f) {
        return algo::Vec2::Zero();
    }
    const float inv_det = 1.0f / det;
    return algo::Vec2{(k22 * rhs.GetX() - k12 * rhs.GetY()) * inv_det,
                      (k11 * rhs.GetY() - k12 * rhs.GetX()) * inv_det};
}

} // namespace

auto RevoluteJoint::Prepare() -> void {
    const algo::Vec2 centroid_a = Centroid(*a);
    const algo::Vec2 centroid_b = Centroid(*b);
    const algo::Vec2 anchor_a = a->position + local_anchor_a.Rotated(a->angle);
    const algo::Vec2 anchor_b = b->position + local_anchor_b.Rotated(b->angle);

    r_a = anchor_a - centroid_a;
    r_b = anchor_b - centroid_b;

    const float i_a = a->inverse_inertia;
    const float i_b = b->inverse_inertia;
    const float m = a->inverse_mass + b->inverse_mass;

    // K = m·I + iA·[rA×]^T[rA×] + iB·[rB×]^T[rB×]（2D 点约束对称形式）。
    k11 = m + i_a * r_a.GetY() * r_a.GetY() + i_b * r_b.GetY() * r_b.GetY();
    k12 = -i_a * r_a.GetX() * r_a.GetY() - i_b * r_b.GetX() * r_b.GetY();
    k22 = m + i_a * r_a.GetX() * r_a.GetX() + i_b * r_b.GetX() * r_b.GetX();
}

auto RevoluteJoint::SolveVelocity() -> void {
    // 相对锚点速度 Cdot = vB + ωB×rB − vA − ωA×rA，驱向 0。
    const algo::Vec2 v_rel = b->linear_velocity + CrossScalarVec(b->angular_velocity, r_b) -
                             a->linear_velocity - CrossScalarVec(a->angular_velocity, r_a);
    const algo::Vec2 impulse = Solve2x2(k11, k12, k22, algo::Vec2{-v_rel.GetX(), -v_rel.GetY()});

    a->linear_velocity -= impulse * a->inverse_mass;
    a->angular_velocity -= a->inverse_inertia * r_a.Cross(impulse);
    b->linear_velocity += impulse * b->inverse_mass;
    b->angular_velocity += b->inverse_inertia * r_b.Cross(impulse);
}

auto RevoluteJoint::SolvePosition() -> void {
    // 位置误差 C = anchorB − anchorA，线性投影消除。
    const algo::Vec2 centroid_a = Centroid(*a);
    const algo::Vec2 centroid_b = Centroid(*b);
    const algo::Vec2 anchor_a = a->position + local_anchor_a.Rotated(a->angle);
    const algo::Vec2 anchor_b = b->position + local_anchor_b.Rotated(b->angle);
    const algo::Vec2 c = anchor_b - anchor_a;

    const algo::Vec2 impulse = Solve2x2(k11, k12, k22, algo::Vec2{-c.GetX(), -c.GetY()});

    // 修正质心与角度，再反解 origin。
    const algo::Vec2 new_centroid_a = centroid_a - impulse * a->inverse_mass;
    const float new_angle_a = a->angle - a->inverse_inertia * r_a.Cross(impulse);
    const algo::Vec2 new_centroid_b = centroid_b + impulse * b->inverse_mass;
    const float new_angle_b = b->angle + b->inverse_inertia * r_b.Cross(impulse);

    a->position = new_centroid_a - a->local_center.Rotated(new_angle_a);
    a->angle = new_angle_a;
    b->position = new_centroid_b - b->local_center.Rotated(new_angle_b);
    b->angle = new_angle_b;
}

} // namespace atom::physics

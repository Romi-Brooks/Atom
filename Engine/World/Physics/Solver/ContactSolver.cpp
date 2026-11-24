// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Solver/ContactSolver.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include <Physics/Collision/NarrowPhase.hpp>
#include <Physics/Dynamics/RigidBody.hpp>

namespace atom::physics {

namespace {

constexpr float kEpsilon = 1.0e-6f;

// 标量叉乘向量：w × r = (-w·r.y, w·r.x)，即旋转的切向速度贡献。
[[nodiscard]] inline auto CrossScalarVec(const float s, const algo::Vec2& v) -> algo::Vec2 {
    return algo::Vec2{-s * v.GetY(), s * v.GetX()};
}

// 质心世界位置。
[[nodiscard]] inline auto WorldCenter(const RigidBody& body) -> algo::Vec2 {
    return body.position + body.local_center.Rotated(body.angle);
}

struct VelocityPoint final {
        algo::Vec2 r_a{};
        algo::Vec2 r_b{};
        float normal_mass = 0.0f;
        float tangent_mass = 0.0f;
        float velocity_bias = 0.0f;
        float normal_impulse = 0.0f;
        float tangent_impulse = 0.0f;
};

struct Constraint final {
        RigidBody* a = nullptr;
        RigidBody* b = nullptr;
        algo::Vec2 normal{};
        float friction = 0.0f;
        VelocityPoint points[2]{};
        int point_count = 0;
};

[[nodiscard]] inline auto BuildConstraint(const ContactManifold& manifold, RigidBody& a, RigidBody& b,
                                          const ContactSolverConfig& config) -> Constraint {
    Constraint c{};
    c.a = &a;
    c.b = &b;
    c.normal = manifold.normal;
    c.friction = std::sqrt(std::max(0.0f, a.material.friction * b.material.friction));
    const float restitution = std::max(a.material.restitution, b.material.restitution);

    const algo::Vec2 center_a = WorldCenter(a);
    const algo::Vec2 center_b = WorldCenter(b);
    const algo::Vec2 n = manifold.normal;
    const algo::Vec2 tangent{n.GetY(), -n.GetX()};

    c.point_count = manifold.point_count;
    for (int i = 0; i < manifold.point_count; ++i) {
        VelocityPoint& p = c.points[i];
        p.r_a = manifold.points[i].point - center_a;
        p.r_b = manifold.points[i].point - center_b;

        // 法线方向有效质量：1 / (inv_m_a + inv_m_b + inv_I_a·(r_a×n)² + inv_I_b·(r_b×n)²)
        const float ra_cross_n = p.r_a.Cross(n);
        const float rb_cross_n = p.r_b.Cross(n);
        const float k_n = a.inverse_mass + b.inverse_mass + a.inverse_inertia * ra_cross_n * ra_cross_n +
                          b.inverse_inertia * rb_cross_n * rb_cross_n;
        p.normal_mass = k_n > 0.0f ? 1.0f / k_n : 0.0f;

        const float ra_cross_t = p.r_a.Cross(tangent);
        const float rb_cross_t = p.r_b.Cross(tangent);
        const float k_t = a.inverse_mass + b.inverse_mass + a.inverse_inertia * ra_cross_t * ra_cross_t +
                          b.inverse_inertia * rb_cross_t * rb_cross_t;
        p.tangent_mass = k_t > 0.0f ? 1.0f / k_t : 0.0f;

        // restitution bias：仅在相对法向速度足够大时启用，避免小幅抖动反弹。
        // 约定：normal 指向 a，接近时 v_rel·normal > 0；分离目标速度 = -restitution·vn（负）。
        const algo::Vec2 v_rel = b.linear_velocity + CrossScalarVec(b.angular_velocity, p.r_b) - a.linear_velocity -
                                 CrossScalarVec(a.angular_velocity, p.r_a);
        const float vn = v_rel.Dot(n);
        p.velocity_bias = vn > config.velocity_threshold ? -restitution * vn : 0.0f;

        p.normal_impulse = 0.0f;
        p.tangent_impulse = 0.0f;
    }
    return c;
}

inline auto SolveVelocity(Constraint& c) -> void {
    RigidBody& a = *c.a;
    RigidBody& b = *c.b;
    const algo::Vec2 n = c.normal;
    const algo::Vec2 tangent{n.GetY(), -n.GetX()};

    for (int i = 0; i < c.point_count; ++i) {
        VelocityPoint& p = c.points[i];

        // --- 摩擦（切向）：冲量限制在 ± μ·normal_impulse ---
        {
            const algo::Vec2 v_rel = b.linear_velocity + CrossScalarVec(b.angular_velocity, p.r_b) -
                                     a.linear_velocity - CrossScalarVec(a.angular_velocity, p.r_a);
            const float vt = v_rel.Dot(tangent);
            const float lambda = vt * p.tangent_mass; // 消除切向相对速度
            const float max_friction = c.friction * p.normal_impulse;
            const float new_impulse = std::clamp(p.tangent_impulse + lambda, -max_friction, max_friction);
            const float delta = new_impulse - p.tangent_impulse;
            p.tangent_impulse = new_impulse;
            const algo::Vec2 impulse = tangent * delta;
            // a 沿 +n 分离、b 沿 -n 分离（法线指向 a 的约定一致施加）。
            a.linear_velocity += impulse * a.inverse_mass;
            a.angular_velocity += a.inverse_inertia * p.r_a.Cross(impulse);
            b.linear_velocity -= impulse * b.inverse_mass;
            b.angular_velocity -= b.inverse_inertia * p.r_b.Cross(impulse);
        }

        // --- 法向：冲量非负，含 restitution bias ---
        {
            const algo::Vec2 v_rel = b.linear_velocity + CrossScalarVec(b.angular_velocity, p.r_b) -
                                     a.linear_velocity - CrossScalarVec(a.angular_velocity, p.r_a);
            const float vn = v_rel.Dot(n);
            const float lambda = (vn - p.velocity_bias) * p.normal_mass; // 把 vn 推向分离目标
            const float new_impulse = std::max(0.0f, p.normal_impulse + lambda);
            const float delta = new_impulse - p.normal_impulse;
            p.normal_impulse = new_impulse;
            const algo::Vec2 impulse = n * delta;
            a.linear_velocity += impulse * a.inverse_mass;
            a.angular_velocity += a.inverse_inertia * p.r_a.Cross(impulse);
            b.linear_velocity -= impulse * b.inverse_mass;
            b.angular_velocity -= b.inverse_inertia * p.r_b.Cross(impulse);
        }
    }
}

} // namespace

auto SolveVelocityContacts(std::vector<ContactSolverInput>& contacts, const int velocity_iterations,
                           const ContactSolverConfig& config) -> void {
    std::vector<Constraint> constraints;
    constraints.reserve(contacts.size());
    for (const ContactSolverInput& input : contacts) {
        if (input.a != nullptr && input.b != nullptr && input.manifold.point_count > 0) {
            constraints.push_back(BuildConstraint(input.manifold, *input.a, *input.b, config));
        }
    }

    for (int iteration = 0; iteration < velocity_iterations; ++iteration) {
        for (Constraint& c : constraints) {
            SolveVelocity(c);
        }
    }
}

auto SolvePositionContacts(std::vector<ContactSolverInput>& contacts, const int position_iterations,
                           const ContactSolverConfig& config) -> void {
    for (int iteration = 0; iteration < position_iterations; ++iteration) {
        for (ContactSolverInput& input : contacts) {
            if (input.a == nullptr || input.b == nullptr) {
                continue;
            }
            RigidBody& a = *input.a;
            RigidBody& b = *input.b;

            // 重算当前穿透（移动后穿透变化，必须基于当前位置）。
            ContactManifold current{};
            if (!Collide(a.shape, a.position, a.angle, b.shape, b.position, b.angle, current)) {
                continue; // 已分离
            }

            const float total_inverse_mass = a.inverse_mass + b.inverse_mass;
            if (total_inverse_mass <= kEpsilon) {
                continue; // 双方都静态，无可移动者
            }

            for (int i = 0; i < current.point_count; ++i) {
                const float penetration = current.points[i].penetration;
                if (penetration <= config.position_slop) {
                    continue;
                }
                // 线性投影修正：把超出的穿透按质量比推离。
                // 法线指向 a：a 沿 +n、b 沿 -n 分离。
                const float correction = config.baumgarte * (penetration - config.position_slop) / total_inverse_mass;
                const algo::Vec2 n = current.normal;
                a.position += n * correction * a.inverse_mass;
                b.position -= n * correction * b.inverse_mass;
            }
        }
    }
}

} // namespace atom::physics

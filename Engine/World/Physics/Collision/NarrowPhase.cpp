// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Collision/NarrowPhase.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace atom::physics {

namespace {

constexpr float kEpsilon = 1.0e-6f;

[[nodiscard]] auto WorldVertex(const ConvexPolygon& p, const algo::Vec2& pos, const float angle,
                               const std::size_t i) -> algo::Vec2 {
    return pos + p.vertices[i].Rotated(angle);
}

// CCW 多边形的面外法线（世界空间单位向量）。
[[nodiscard]] auto FaceNormal(const algo::Vec2& v0, const algo::Vec2& v1) -> algo::Vec2 {
    const algo::Vec2 edge = v1 - v0;
    return algo::Vec2{edge.GetY(), -edge.GetX()}.Normalized();
}

// 把 2 点线段裁剪到半平面 normal·p - offset <= 0，输出保留的顶点，返回数量。
[[nodiscard]] auto ClipSegment(std::array<algo::Vec2, 2>& out, const std::array<algo::Vec2, 2>& in,
                               const algo::Vec2& normal, const float offset) -> int {
    int count = 0;
    const float d0 = normal.Dot(in[0]) - offset;
    const float d1 = normal.Dot(in[1]) - offset;
    if (d0 <= 0.0f) {
        out[count++] = in[0];
    }
    if (d1 <= 0.0f) {
        out[count++] = in[1];
    }
    if (d0 * d1 < 0.0f) {
        const float t = d0 / (d0 - d1);
        out[count++] = in[0] + (in[1] - in[0]) * t;
    }
    return count;
}

[[nodiscard]] auto CollideCircleCircle(const Circle& ca, const algo::Vec2 pa, const float aa, const Circle& cb,
                                       const algo::Vec2 pb, const float ab, ContactManifold& manifold) -> bool {
    const algo::Vec2 center_a = pa + ca.center.Rotated(aa);
    const algo::Vec2 center_b = pb + cb.center.Rotated(ab);
    const algo::Vec2 d = center_b - center_a; // a -> b
    const float radius_sum = ca.radius + cb.radius;
    const float dist2 = d.LengthSquared();
    if (dist2 >= radius_sum * radius_sum) {
        return false;
    }
    const float dist = std::sqrt(dist2);
    const algo::Vec2 n = dist > kEpsilon ? d / dist : algo::Vec2{0.0f, 1.0f};

    manifold.normal = -n; // 指向 a
    manifold.point_count = 1;
    manifold.points[0].penetration = radius_sum - dist;
    manifold.points[0].point = center_a + n * (ca.radius - manifold.points[0].penetration * 0.5f);
    return true;
}

// 圆 vs 多边形，法线从多边形指向圆（指向「圆」这个形状）。
[[nodiscard]] auto CollideCirclePolygon(const Circle& circle, const algo::Vec2 circle_center, const float radius,
                                        const ConvexPolygon& poly, const algo::Vec2 poly_pos, const float poly_angle,
                                        ContactManifold& manifold) -> bool {
    const algo::Vec2 c = (circle_center - poly_pos).Rotated(-poly_angle); // 圆心到多边形局部空间

    float separation = -std::numeric_limits<float>::max();
    int face = 0;
    for (std::size_t i = 0; i < poly.count; ++i) {
        const algo::Vec2& a = poly.vertices[i];
        const algo::Vec2& b = poly.vertices[(i + 1) % poly.count];
        const algo::Vec2 outward = FaceNormal(a, b);
        const float s = outward.Dot(c - a);
        if (s > radius) {
            return false; // 圆心在该面外侧超过半径，分离
        }
        if (s > separation) {
            separation = s;
            face = static_cast<int>(i);
        }
    }

    const algo::Vec2 v1 = poly.vertices[face];
    const algo::Vec2 v2 = poly.vertices[(face + 1) % poly.count];
    const algo::Vec2 outward = FaceNormal(v1, v2);

    algo::Vec2 local_normal{};
    algo::Vec2 local_point{};
    float penetration = 0.0f;

    if (separation < kEpsilon) {
        // 圆心在多边形内部：接触点取圆心向最近面的投影（随圆移动，避免固定参考点产生虚假力臂）。
        local_normal = outward;
        local_point = c - outward * separation;
        penetration = radius - separation;
    } else {
        // 圆心在多边形外，判断落在该面的顶点 voronoi 区域还是面内部。
        const float u1 = (c - v1).Dot(v2 - v1);
        const float u2 = (c - v2).Dot(v1 - v2);
        if (u1 <= 0.0f) {
            if ((c - v1).LengthSquared() > radius * radius) {
                return false;
            }
            local_normal = (c - v1).Normalized();
            local_point = v1;
            penetration = radius - (c - v1).Length();
        } else if (u2 <= 0.0f) {
            if ((c - v2).LengthSquared() > radius * radius) {
                return false;
            }
            local_normal = (c - v2).Normalized();
            local_point = v2;
            penetration = radius - (c - v2).Length();
        } else {
            const float s = separation; // 面上最近点 = 圆心沿法线投影，接触点必须随圆心移动
            local_normal = outward;
            local_point = c - outward * s;
            penetration = radius - s;
        }
    }

    manifold.normal = local_normal.Rotated(poly_angle); // 世界空间，指向圆
    manifold.point_count = 1;
    manifold.points[0].point = poly_pos + local_point.Rotated(poly_angle);
    manifold.points[0].penetration = penetration;
    return true;
}

[[nodiscard]] auto CollidePolygonPolygon(const ConvexPolygon& a, const algo::Vec2 pa, const float aa,
                                         const ConvexPolygon& b, const algo::Vec2 pb, const float ab,
                                         ContactManifold& manifold) -> bool {
    const float neg_max = -std::numeric_limits<float>::max();

    // 找 A 的最小分离面。
    float sep_a = neg_max;
    int face_a = 0;
    for (std::size_t i = 0; i < a.count; ++i) {
        const algo::Vec2 v0 = WorldVertex(a, pa, aa, i);
        const algo::Vec2 n = FaceNormal(v0, WorldVertex(a, pa, aa, (i + 1) % a.count));
        float min_s = std::numeric_limits<float>::max();
        for (std::size_t j = 0; j < b.count; ++j) {
            min_s = std::min(min_s, n.Dot(WorldVertex(b, pb, ab, j) - v0));
        }
        if (min_s > 0.0f) {
            return false;
        }
        if (min_s > sep_a) {
            sep_a = min_s;
            face_a = static_cast<int>(i);
        }
    }

    // 找 B 的最小分离面。
    float sep_b = neg_max;
    int face_b = 0;
    for (std::size_t i = 0; i < b.count; ++i) {
        const algo::Vec2 v0 = WorldVertex(b, pb, ab, i);
        const algo::Vec2 n = FaceNormal(v0, WorldVertex(b, pb, ab, (i + 1) % b.count));
        float min_s = std::numeric_limits<float>::max();
        for (std::size_t j = 0; j < a.count; ++j) {
            min_s = std::min(min_s, n.Dot(WorldVertex(a, pa, aa, j) - v0));
        }
        if (min_s > 0.0f) {
            return false;
        }
        if (min_s > sep_b) {
            sep_b = min_s;
            face_b = static_cast<int>(i);
        }
    }

    const bool reference_is_a = sep_a >= sep_b;

    // reference face（世界坐标 + 法线，法线从 reference 指向 incident）。
    const algo::Vec2 rv0 = reference_is_a ? WorldVertex(a, pa, aa, face_a) : WorldVertex(b, pb, ab, face_b);
    const algo::Vec2 rv1 = reference_is_a ? WorldVertex(a, pa, aa, (face_a + 1) % a.count)
                                          : WorldVertex(b, pb, ab, (face_b + 1) % b.count);
    const algo::Vec2 ref_normal = FaceNormal(rv0, rv1);

    // incident face：另一个多边形上法线最接近 -ref_normal 的面。
    const ConvexPolygon& inc_poly = reference_is_a ? b : a;
    const algo::Vec2 inc_pos = reference_is_a ? pb : pa;
    const float inc_angle = reference_is_a ? ab : aa;
    int incident_face = 0;
    float min_dot = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < inc_poly.count; ++i) {
        const algo::Vec2 v0 = WorldVertex(inc_poly, inc_pos, inc_angle, i);
        const algo::Vec2 n = FaceNormal(v0, WorldVertex(inc_poly, inc_pos, inc_angle, (i + 1) % inc_poly.count));
        const float d = n.Dot(ref_normal);
        if (d < min_dot) {
            min_dot = d;
            incident_face = static_cast<int>(i);
        }
    }

    // 把 incident 边裁剪到 reference 的两个侧平面。
    const std::array<algo::Vec2, 2> incident_edge = {
        WorldVertex(inc_poly, inc_pos, inc_angle, incident_face),
        WorldVertex(inc_poly, inc_pos, inc_angle, (incident_face + 1) % inc_poly.count)};
    const algo::Vec2 tangent = (rv1 - rv0).Normalized();

    std::array<algo::Vec2, 2> clip1{};
    if (ClipSegment(clip1, incident_edge, -tangent, -tangent.Dot(rv0)) < 2) {
        return false;
    }
    std::array<algo::Vec2, 2> clip2{};
    if (ClipSegment(clip2, clip1, tangent, tangent.Dot(rv1)) < 2) {
        return false;
    }

    // 保留 reference face 内侧（separation <= 0）的接触点。
    int point_count = 0;
    for (const algo::Vec2& p : clip2) {
        const float s = ref_normal.Dot(p - rv0);
        if (s <= 0.0f) {
            manifold.points[point_count].point = p;
            manifold.points[point_count].penetration = -s;
            ++point_count;
        }
    }

    manifold.point_count = point_count;
    manifold.normal = reference_is_a ? -ref_normal : ref_normal; // 指向 a
    return point_count > 0;
}

} // namespace

auto Collide(const Shape& a, const algo::Vec2 position_a, const float angle_a, const Shape& b, const algo::Vec2 position_b,
             const float angle_b, ContactManifold& manifold) -> bool {
    const Shape::Kind ka = a.GetKind();
    const Shape::Kind kb = b.GetKind();

    if (ka == Shape::Kind::Circle && kb == Shape::Kind::Circle) {
        return CollideCircleCircle(a.GetCircle(), position_a, angle_a, b.GetCircle(), position_b, angle_b, manifold);
    }
    if (ka == Shape::Kind::Circle && kb == Shape::Kind::Polygon) {
        const Circle& c = a.GetCircle();
        return CollideCirclePolygon(c, position_a + c.center.Rotated(angle_a), c.radius, b.GetPolygon(), position_b,
                                    angle_b, manifold);
    }
    if (ka == Shape::Kind::Polygon && kb == Shape::Kind::Circle) {
        const Circle& c = b.GetCircle();
        const bool hit = CollideCirclePolygon(c, position_b + c.center.Rotated(angle_b), c.radius, a.GetPolygon(),
                                              position_a, angle_a, manifold);
        if (hit) {
            manifold.normal = -manifold.normal; // 反转为指向 a（多边形）
        }
        return hit;
    }
    if (ka == Shape::Kind::Polygon && kb == Shape::Kind::Polygon) {
        return CollidePolygonPolygon(a.GetPolygon(), position_a, angle_a, b.GetPolygon(), position_b, angle_b, manifold);
    }
    // Segment 暂不参与窄相位。
    return false;
}

} // namespace atom::physics

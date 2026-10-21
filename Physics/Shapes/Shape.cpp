// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Shapes/Shape.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <utility>

#include <Algorithm/Math/Scalar.hpp>

namespace atom::physics {

namespace {

constexpr float kEpsilon = 1.0e-6f;

[[nodiscard]] auto InvalidShape(std::string detail) -> std::unexpected<atom::Error> {
    return std::unexpected<atom::Error>{
        atom::Error{atom::ErrorCode::PhysicsInvalidShape, atom::Module::Physics, "MakeShape", std::move(detail)}};
}

} // namespace

// --- factories ---------------------------------------------------------------

auto Shape::MakeCircle(const float radius) -> atom::Result<Shape> {
    if (radius <= kEpsilon) {
        return InvalidShape("circle radius must be positive");
    }
    Shape shape{};
    shape.kind_ = Kind::Circle;
    shape.circle_.radius = radius;
    return shape;
}

auto Shape::MakePolygon(std::span<const algo::Vec2> vertices) -> atom::Result<Shape> {
    if (vertices.size() < 3) {
        return InvalidShape("polygon needs at least 3 vertices");
    }
    if (vertices.size() > ConvexPolygon::kMaxVertices) {
        return InvalidShape("polygon exceeds 8 vertices");
    }

    // 有向面积的两倍（shoelace）。符号给出环绕方向：CCW 为正，CW 为负。
    float cross_sum = 0.0f;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        cross_sum += vertices[i].Cross(vertices[(i + 1) % vertices.size()]);
    }
    if (std::abs(cross_sum) <= kEpsilon) {
        return InvalidShape("polygon is degenerate (zero area)");
    }

    // 凸性：相邻边叉积必须同号（相对第一个非零边叉积）。共线（叉积为 0）容忍。
    float reference = 0.0f;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const algo::Vec2& a = vertices[i];
        const algo::Vec2& b = vertices[(i + 1) % vertices.size()];
        const algo::Vec2& c = vertices[(i + 2) % vertices.size()];
        const float edge_cross = (b - a).Cross(c - b);
        if (reference == 0.0f) {
            if (std::abs(edge_cross) > kEpsilon) {
                reference = edge_cross;
            }
        } else if (edge_cross * reference < -kEpsilon) {
            return std::unexpected<atom::Error>{atom::Error{atom::ErrorCode::PhysicsNonConvexPolygon,
                                                            atom::Module::Physics, "MakeShape", "polygon is concave"}};
        }
    }

    Shape shape{};
    shape.kind_ = Kind::Polygon;
    shape.polygon_.count = vertices.size();
    if (cross_sum > 0.0f) {
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            shape.polygon_.vertices[i] = vertices[i];
        }
    } else {
        // CW 输入：反转顶点序，统一为 CCW 存储。
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            shape.polygon_.vertices[i] = vertices[vertices.size() - 1 - i];
        }
    }
    return shape;
}

auto Shape::MakeSegment(algo::Vec2 a, algo::Vec2 b) -> atom::Result<Shape> {
    if ((b - a).LengthSquared() <= kEpsilon * kEpsilon) {
        return InvalidShape("segment has zero length");
    }
    Shape shape{};
    shape.kind_ = Kind::Segment;
    shape.segment_.a = a;
    shape.segment_.b = b;
    return shape;
}

// --- accessors ---------------------------------------------------------------

auto Shape::GetCircle() const -> const Circle& {
    assert(kind_ == Kind::Circle);
    return circle_;
}

auto Shape::GetPolygon() const -> const ConvexPolygon& {
    assert(kind_ == Kind::Polygon);
    return polygon_;
}

auto Shape::GetSegment() const -> const Segment& {
    assert(kind_ == Kind::Segment);
    return segment_;
}

// --- geometry ----------------------------------------------------------------

auto ComputeArea(const Shape& shape) -> float {
    switch (shape.GetKind()) {
    case Shape::Kind::Circle: {
        const float r = shape.GetCircle().radius;
        return algo::Pi * r * r;
    }
    case Shape::Kind::Polygon: {
        const auto& p = shape.GetPolygon();
        float cross_sum = 0.0f;
        for (std::size_t i = 0; i < p.count; ++i) {
            cross_sum += p.vertices[i].Cross(p.vertices[(i + 1) % p.count]);
        }
        return 0.5f * cross_sum;
    }
    case Shape::Kind::Segment:
        return 0.0f;
    }
    return 0.0f;
}

auto ComputeCentroid(const Shape& shape) -> algo::Vec2 {
    switch (shape.GetKind()) {
    case Shape::Kind::Circle:
        return shape.GetCircle().center;
    case Shape::Kind::Polygon: {
        const auto& p = shape.GetPolygon();
        float cross_sum = 0.0f;
        algo::Vec2 weighted_sum{};
        for (std::size_t i = 0; i < p.count; ++i) {
            const float cross = p.vertices[i].Cross(p.vertices[(i + 1) % p.count]);
            cross_sum += cross;
            weighted_sum += (p.vertices[i] + p.vertices[(i + 1) % p.count]) * cross;
        }
        // centroid = Σ(p_i + p_j)·cross / (6·area)，area = cross_sum/2。
        return weighted_sum / (3.0f * cross_sum);
    }
    case Shape::Kind::Segment: {
        const auto& s = shape.GetSegment();
        return (s.a + s.b) * 0.5f;
    }
    }
    return {};
}

auto ComputeAabb(const Shape& shape) -> algo::Aabb2 {
    algo::Aabb2 aabb{};
    switch (shape.GetKind()) {
    case Shape::Kind::Circle: {
        const auto& c = shape.GetCircle();
        const algo::Vec2 r{c.radius, c.radius};
        aabb.min = c.center - r;
        aabb.max = c.center + r;
        break;
    }
    case Shape::Kind::Polygon: {
        const auto& p = shape.GetPolygon();
        aabb.min = p.vertices[0];
        aabb.max = p.vertices[0];
        for (std::size_t i = 1; i < p.count; ++i) {
            aabb.min = algo::Vec2{std::min(aabb.min.GetX(), p.vertices[i].GetX()),
                                  std::min(aabb.min.GetY(), p.vertices[i].GetY())};
            aabb.max = algo::Vec2{std::max(aabb.max.GetX(), p.vertices[i].GetX()),
                                  std::max(aabb.max.GetY(), p.vertices[i].GetY())};
        }
        break;
    }
    case Shape::Kind::Segment: {
        const auto& s = shape.GetSegment();
        aabb.min = algo::Vec2{std::min(s.a.GetX(), s.b.GetX()), std::min(s.a.GetY(), s.b.GetY())};
        aabb.max = algo::Vec2{std::max(s.a.GetX(), s.b.GetX()), std::max(s.a.GetY(), s.b.GetY())};
        break;
    }
    }
    return aabb;
}

auto ComputeMassProperties(const Shape& shape, const float density) -> MassProperties {
    MassProperties result{};

    const float area = ComputeArea(shape);
    const float mass = density * area;
    const algo::Vec2 centroid = ComputeCentroid(shape);

    float inertia = 0.0f;
    switch (shape.GetKind()) {
    case Shape::Kind::Circle: {
        const float r = shape.GetCircle().radius;
        inertia = 0.5f * mass * r * r;
        break;
    }
    case Shape::Kind::Polygon: {
        const auto& p = shape.GetPolygon();
        // 绕局部原点的惯性：I = ρ/12 · Σ cross(v_i, v_j) · (v_i² + v_i·v_j + v_j²)
        float origin_inertia = 0.0f;
        for (std::size_t i = 0; i < p.count; ++i) {
            const auto& v0 = p.vertices[i];
            const auto& v1 = p.vertices[(i + 1) % p.count];
            origin_inertia += v0.Cross(v1) * (v0.Dot(v0) + v0.Dot(v1) + v1.Dot(v1));
        }
        origin_inertia *= density / 12.0f;
        // 平行轴定理移到质心。
        inertia = origin_inertia - mass * centroid.LengthSquared();
        break;
    }
    case Shape::Kind::Segment:
        inertia = 0.0f;
        break;
    }

    result.mass = mass;
    result.inertia = inertia > 0.0f ? inertia : 0.0f;
    result.inverse_mass = mass > 0.0f ? 1.0f / mass : 0.0f;
    result.inverse_inertia = result.inertia > 0.0f ? 1.0f / result.inertia : 0.0f;
    result.centroid = centroid;
    return result;
}

} // namespace atom::physics

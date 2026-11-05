#include <Algorithm/Math/Scalar.hpp>
#include <Physics/Shapes/Material.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <array>
#include <limits>
#include <vector>

auto main() -> int {
    using namespace atom::physics;
    using atom::algo::Vec2;

    // 工厂拒绝 NaN/Inf，避免把非法数据带入 AABB/求解器。
    ATOM_CHECK(!Shape::MakeCircle(std::numeric_limits<float>::quiet_NaN()));
    const std::array<Vec2, 3> non_finite = {Vec2{0.0f, 0.0f},
                                             Vec2{std::numeric_limits<float>::infinity(), 0.0f},
                                             Vec2{0.0f, 1.0f}};
    ATOM_CHECK(!Shape::MakePolygon(non_finite));
    ATOM_CHECK(!Shape::MakeSegment(Vec2{0.0f, 0.0f}, Vec2{std::numeric_limits<float>::infinity(), 0.0f}));

    // --- 圆 ---
    auto circle = Shape::MakeCircle(1.0f);
    ATOM_CHECK(circle.has_value());
    ATOM_CHECK(circle->GetKind() == Shape::Kind::Circle);
    ATOM_CHECK_NEAR(ComputeArea(*circle), atom::algo::Pi, 1.0e-4f);
    ATOM_CHECK(ComputeCentroid(*circle) == Vec2{0.0f, 0.0f});
    {
        const auto mp = ComputeMassProperties(*circle, 1.0f);
        ATOM_CHECK_NEAR(mp.mass, atom::algo::Pi, 1.0e-4f);
        ATOM_CHECK_NEAR(mp.inertia, 0.5f * atom::algo::Pi, 1.0e-4f); // ½·m·r²
        ATOM_CHECK_NEAR(mp.inverse_mass, 1.0f / atom::algo::Pi, 1.0e-4f);
    }
    {
        const auto aabb = ComputeAabb(*circle);
        ATOM_CHECK_NEAR(aabb.min.GetX(), -1.0f, 1.0e-5f);
        ATOM_CHECK_NEAR(aabb.max.GetX(), 1.0f, 1.0e-5f);
    }

    // --- 正方形（中心在原点，CCW）---
    const std::array<Vec2, 4> square = {Vec2{-0.5f, -0.5f}, Vec2{0.5f, -0.5f}, Vec2{0.5f, 0.5f}, Vec2{-0.5f, 0.5f}};
    auto poly = Shape::MakePolygon(square);
    ATOM_CHECK(poly.has_value());
    ATOM_CHECK_NEAR(ComputeArea(*poly), 1.0f, 1.0e-5f);
    ATOM_CHECK(ComputeCentroid(*poly) == Vec2{0.0f, 0.0f});
    {
        const auto mp = ComputeMassProperties(*poly, 1.0f);
        ATOM_CHECK_NEAR(mp.mass, 1.0f, 1.0e-5f);
        ATOM_CHECK_NEAR(mp.inertia, 1.0f / 6.0f, 1.0e-4f); // 正方形绕中心 = m·s²/6
    }

    // --- 三角形（重心 = 顶点平均）---
    const std::array<Vec2, 3> tri = {Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec2{0.0f, 1.0f}};
    auto t = Shape::MakePolygon(tri);
    ATOM_CHECK(t.has_value());
    ATOM_CHECK_NEAR(ComputeArea(*t), 0.5f, 1.0e-5f);
    ATOM_CHECK_NEAR(ComputeCentroid(*t).GetX(), 1.0f / 3.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(ComputeCentroid(*t).GetY(), 1.0f / 3.0f, 1.0e-5f);

    // --- CW 输入自动反转为 CCW（面积仍为正）---
    const std::array<Vec2, 4> cw = {Vec2{-0.5f, -0.5f}, Vec2{-0.5f, 0.5f}, Vec2{0.5f, 0.5f}, Vec2{0.5f, -0.5f}};
    auto cwp = Shape::MakePolygon(cw);
    ATOM_CHECK(cwp.has_value());
    ATOM_CHECK_NEAR(ComputeArea(*cwp), 1.0f, 1.0e-5f);

    // --- 非法输入（通过 Result 上报，而非静默产生退化形状）---
    ATOM_CHECK(!Shape::MakeCircle(0.0f).has_value());
    ATOM_CHECK(!Shape::MakeCircle(-1.0f).has_value());
    ATOM_CHECK(!Shape::MakeSegment(Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}).has_value());

    const std::array<Vec2, 2> two = {Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}};
    ATOM_CHECK(!Shape::MakePolygon(two).has_value());

    const std::vector<Vec2> nine(9, Vec2{0.0f, 0.0f});
    ATOM_CHECK(!Shape::MakePolygon(nine).has_value());

    const std::array<Vec2, 4> concave = {Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec2{0.3f, 0.3f}, Vec2{0.0f, 1.0f}};
    ATOM_CHECK(!Shape::MakePolygon(concave).has_value());

    // --- 线段（无面积、无质量）---
    auto seg = Shape::MakeSegment(Vec2{-1.0f, 0.0f}, Vec2{1.0f, 0.0f});
    ATOM_CHECK(seg.has_value());
    ATOM_CHECK(seg->GetKind() == Shape::Kind::Segment);
    ATOM_CHECK_NEAR(ComputeArea(*seg), 0.0f, 0.0f);
    {
        const auto mp = ComputeMassProperties(*seg, 1.0f);
        ATOM_CHECK_NEAR(mp.mass, 0.0f, 0.0f);
        ATOM_CHECK_NEAR(mp.inertia, 0.0f, 0.0f);
    }
    {
        const auto aabb = ComputeAabb(*seg);
        ATOM_CHECK_NEAR(aabb.min.GetX(), -1.0f, 0.0f);
        ATOM_CHECK_NEAR(aabb.max.GetX(), 1.0f, 0.0f);
        ATOM_CHECK_NEAR(aabb.min.GetY(), 0.0f, 0.0f);
    }

    // --- Material 默认值 ---
    const Material mat{};
    ATOM_CHECK_NEAR(mat.density, 1.0f, 0.0f);
    ATOM_CHECK_NEAR(mat.friction, 0.2f, 0.0f);
    ATOM_CHECK_NEAR(mat.restitution, 0.0f, 0.0f);

    return 0;
}

#include <array>

#include <Physics/Collision/NarrowPhase.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::physics;
    using atom::algo::Vec2;

    const auto circle1 = *Shape::MakeCircle(1.0f);
    const std::array<Vec2, 4> square = {Vec2{-1.0f, -1.0f}, Vec2{1.0f, -1.0f}, Vec2{1.0f, 1.0f}, Vec2{-1.0f, 1.0f}};
    const auto square_poly = *Shape::MakePolygon(square);

    // --- 圆-圆：相交 + 法线方向 + 穿透 ---
    {
        ContactManifold m{};
        const bool hit = Collide(circle1, Vec2{0.0f, 0.0f}, 0.0f, circle1, Vec2{0.0f, 1.5f}, 0.0f, m);
        ATOM_CHECK(hit);
        ATOM_CHECK(m.point_count == 1);
        ATOM_CHECK_NEAR(m.points[0].penetration, 0.5f, 1.0e-5f);
        // normal 指向 a（下方），从 b（上方）指向 a。
        ATOM_CHECK_NEAR(m.normal.GetY(), -1.0f, 1.0e-5f);
    }

    // --- 圆-圆：分离 ---
    {
        ContactManifold m{};
        ATOM_CHECK(!Collide(circle1, Vec2{0.0f, 0.0f}, 0.0f, circle1, Vec2{0.0f, 3.0f}, 0.0f, m));
    }

    // --- 圆-多边形：圆在上方，法线指向圆（+y）---
    {
        ContactManifold m{};
        const bool hit = Collide(circle1, Vec2{0.0f, 1.5f}, 0.0f, square_poly, Vec2{0.0f, 0.0f}, 0.0f, m);
        ATOM_CHECK(hit);
        ATOM_CHECK(m.point_count == 1);
        ATOM_CHECK_NEAR(m.points[0].penetration, 0.5f, 1.0e-5f);
        ATOM_CHECK_NEAR(m.normal.GetY(), 1.0f, 1.0e-5f);
    }

    // --- 圆-多边形：分离 ---
    {
        ContactManifold m{};
        ATOM_CHECK(!Collide(circle1, Vec2{0.0f, 3.0f}, 0.0f, square_poly, Vec2{0.0f, 0.0f}, 0.0f, m));
    }

    // --- 多边形-多边形：重叠 + 法线 + 穿透 ---
    {
        ContactManifold m{};
        const bool hit = Collide(square_poly, Vec2{0.0f, 0.0f}, 0.0f, square_poly, Vec2{0.0f, 1.5f}, 0.0f, m);
        ATOM_CHECK(hit);
        ATOM_CHECK(m.point_count >= 1);
        // B 在 A 上方，normal 指向 A（下方）。
        ATOM_CHECK_NEAR(m.normal.GetY(), -1.0f, 1.0e-4f);
        ATOM_CHECK_NEAR(m.points[0].penetration, 0.5f, 1.0e-4f);
    }

    // --- 多边形-多边形：分离 ---
    {
        ContactManifold m{};
        ATOM_CHECK(!Collide(square_poly, Vec2{0.0f, 0.0f}, 0.0f, square_poly, Vec2{0.0f, 3.0f}, 0.0f, m));
    }

    // --- 旋转多边形仍能检测碰撞（SAT 对旋转正确）---
    {
        ContactManifold m{};
        // A 旋转 45°，中心 (0,0)，仍与 B 重叠。
        const bool hit = Collide(square_poly, Vec2{0.0f, 0.0f}, 0.785398f, square_poly, Vec2{0.0f, 1.2f}, 0.0f, m);
        ATOM_CHECK(hit);
    }

    // --- 三角形 vs 三角形 ---
    {
        const std::array<Vec2, 3> tri = {Vec2{0.0f, 0.0f}, Vec2{2.0f, 0.0f}, Vec2{0.0f, 2.0f}};
        const auto tri_a = *Shape::MakePolygon(tri);
        const auto tri_b = *Shape::MakePolygon(tri);
        ContactManifold m{};
        ATOM_CHECK(Collide(tri_a, Vec2{0.0f, 0.0f}, 0.0f, tri_b, Vec2{1.0f, 0.0f}, 0.0f, m));
        ContactManifold m2{};
        ATOM_CHECK(!Collide(tri_a, Vec2{0.0f, 0.0f}, 0.0f, tri_b, Vec2{3.0f, 3.0f}, 0.0f, m2));
    }

    return 0;
}

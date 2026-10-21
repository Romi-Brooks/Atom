#include <Physics/Dynamics/RigidBody.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::physics;
    using atom::algo::Vec2;

    // --- 自由落体：动态体，重力 (0,-10)，dt=0.1 ---
    {
        RigidBody body{};
        body.type = BodyType::Dynamic;
        body.shape = *Shape::MakeCircle(1.0f);
        body.mass = 1.0f;
        body.inverse_mass = 1.0f;
        body.inertia = 0.5f;
        body.inverse_inertia = 2.0f;

        Integrate(body, 0.1f, Vec2{0.0f, -10.0f});
        ATOM_CHECK_NEAR(body.linear_velocity.GetY(), -1.0f, 1.0e-5f);
        ATOM_CHECK_NEAR(body.position.GetY(), -0.1f, 1.0e-5f);
        ATOM_CHECK(body.force == Vec2::Zero()); // 积分后累积力清零
    }

    // --- 抛体：初速度 (1,0)，无重力 → 匀速直线 ---
    {
        RigidBody body{};
        body.type = BodyType::Dynamic;
        body.shape = *Shape::MakeCircle(1.0f);
        body.mass = 1.0f;
        body.inverse_mass = 1.0f;
        body.inertia = 1.0f;
        body.inverse_inertia = 1.0f;
        body.linear_velocity = Vec2{1.0f, 0.0f};
        for (int i = 0; i < 10; ++i) {
            Integrate(body, 0.1f, Vec2::Zero());
        }
        ATOM_CHECK_NEAR(body.position.GetX(), 1.0f, 1.0e-4f);
        ATOM_CHECK_NEAR(body.position.GetY(), 0.0f, 1.0e-4f);
    }

    // --- static 不动，速度被清零 ---
    {
        RigidBody body{};
        body.type = BodyType::Static;
        body.shape = *Shape::MakeCircle(1.0f);
        body.linear_velocity = Vec2{5.0f, 5.0f};
        Integrate(body, 0.1f, Vec2{0.0f, -10.0f});
        ATOM_CHECK(body.position == Vec2::Zero());
        ATOM_CHECK(body.linear_velocity == Vec2::Zero());
    }

    // --- kinematic 只积分位置，不受重力 ---
    {
        RigidBody body{};
        body.type = BodyType::Kinematic;
        body.shape = *Shape::MakeCircle(1.0f);
        body.linear_velocity = Vec2{2.0f, 0.0f};
        Integrate(body, 0.1f, Vec2{0.0f, -10.0f});
        ATOM_CHECK_NEAR(body.position.GetX(), 0.2f, 1.0e-5f);
        ATOM_CHECK_NEAR(body.position.GetY(), 0.0f, 1.0e-5f);
        ATOM_CHECK_NEAR(body.linear_velocity.GetY(), 0.0f, 1.0e-5f);
    }

    // --- 确定性：两次相同积分结果逐位一致 ---
    {
        RigidBody a{};
        RigidBody b{};
        a.type = b.type = BodyType::Dynamic;
        a.shape = b.shape = *Shape::MakeCircle(1.0f);
        a.mass = b.mass = 1.0f;
        a.inverse_mass = b.inverse_mass = 1.0f;
        a.inertia = b.inertia = 0.5f;
        a.inverse_inertia = b.inverse_inertia = 2.0f;
        for (int i = 0; i < 100; ++i) {
            Integrate(a, 1.0f / 60.0f, Vec2{0.0f, -9.8f});
            Integrate(b, 1.0f / 60.0f, Vec2{0.0f, -9.8f});
        }
        ATOM_CHECK(a.position == b.position);
        ATOM_CHECK(a.linear_velocity == b.linear_velocity);
    }

    // --- ComputeWorldAabb：圆平移 ---
    {
        const auto shape = *Shape::MakeCircle(1.0f);
        const auto aabb = ComputeWorldAabb(shape, Vec2{3.0f, 4.0f}, 0.0f);
        ATOM_CHECK_NEAR(aabb.min.GetX(), 2.0f, 1.0e-5f);
        ATOM_CHECK_NEAR(aabb.max.GetX(), 4.0f, 1.0e-5f);
        ATOM_CHECK_NEAR(aabb.min.GetY(), 3.0f, 1.0e-5f);
        ATOM_CHECK_NEAR(aabb.max.GetY(), 5.0f, 1.0e-5f);
    }

    return 0;
}

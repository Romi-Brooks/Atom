#include <array>
#include <cmath>

#include <Physics/Contracts/IPhysicsBody.hpp>
#include <Physics/Contracts/IPhysicsJoint.hpp>
#include <Physics/Contracts/IPhysicsWorld.hpp>
#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>
#include <Physics/Contracts/PhysicsTypes.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

namespace {

using atom::algo::Vec2;

[[nodiscard]] auto Dist(const Vec2& a, const Vec2& b) -> float {
    const Vec2 d = a - b;
    return std::sqrt(d.LengthSquared());
}

} // namespace

auto main() -> int {
    using namespace atom::physics;

    InternalPhysicsBackend backend{};

    // --- 单摆：球被铰链钉在静态锚点，摆动期间锚点始终重合、距离守恒 ---
    {
        auto world = backend.CreateWorld();

        BodyDef pivot_def{};
        pivot_def.type = BodyType::Static;
        pivot_def.shape = *Shape::MakeCircle(0.1f);
        pivot_def.position = Vec2{0.0f, 0.0f};
        auto* pivot = world->CreateBody(pivot_def);
        ATOM_CHECK(pivot != nullptr);

        BodyDef ball_def{};
        ball_def.type = BodyType::Dynamic;
        ball_def.shape = *Shape::MakeCircle(0.5f);
        ball_def.position = Vec2{0.5f, 0.0f};
        auto* ball = world->CreateBody(ball_def);
        ATOM_CHECK(ball != nullptr);

        RevoluteJointDef joint_def{};
        joint_def.local_anchor_a = Vec2{0.0f, 0.0f};
        joint_def.local_anchor_b = Vec2{-0.5f, 0.0f};
        joint_def.collide_connected = false; // 球与锚点不碰撞
        auto* joint = world->CreateRevoluteJoint(joint_def, pivot, ball);
        ATOM_CHECK(joint != nullptr);

        for (int i = 0; i < 240; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
            if (i % 20 == 0) {
                // 铰链两锚点应保持重合。
                ATOM_CHECK_NEAR(Dist(joint->GetAnchorA(), joint->GetAnchorB()), 0.0f, 0.05f);
                // 球心到枢轴距离应保持在半径 0.5 附近（不飞走、不塌缩）。
                ATOM_CHECK_NEAR(Dist(ball->GetPosition(), Vec2{0.0f, 0.0f}), 0.5f, 0.06f);
            }
        }
    }

    // --- 双动态体铰链链：两方块在重力下共同下落，锚点始终重合 ---
    {
        auto world = backend.CreateWorld();

        BodyDef box_def{};
        box_def.type = BodyType::Dynamic;
        const std::array<Vec2, 4> sq = {Vec2{-0.5f, -0.5f}, Vec2{0.5f, -0.5f}, Vec2{0.5f, 0.5f}, Vec2{-0.5f, 0.5f}};
        box_def.shape = *Shape::MakePolygon(sq);

        box_def.position = Vec2{0.0f, 0.0f};
        auto* box_a = world->CreateBody(box_def);
        box_def.position = Vec2{1.0f, 0.0f};
        auto* box_b = world->CreateBody(box_def);
        ATOM_CHECK(box_a != nullptr && box_b != nullptr);

        RevoluteJointDef joint_def{};
        joint_def.local_anchor_a = Vec2{0.5f, 0.0f};  // A 右边缘
        joint_def.local_anchor_b = Vec2{-0.5f, 0.0f}; // B 左边缘
        joint_def.collide_connected = false;
        auto* joint = world->CreateRevoluteJoint(joint_def, box_a, box_b);
        ATOM_CHECK(joint != nullptr);

        const float y0 = box_a->GetPosition().GetY();
        for (int i = 0; i < 120; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
            if (i % 20 == 0) {
                ATOM_CHECK_NEAR(Dist(joint->GetAnchorA(), joint->GetAnchorB()), 0.0f, 0.05f);
            }
        }
        // 铰链在重力作用下整体下落。
        ATOM_CHECK(box_a->GetPosition().GetY() < y0);
    }

    // --- 非法关节创建被拒绝 ---
    {
        auto world = backend.CreateWorld();
        BodyDef def{};
        def.type = BodyType::Dynamic;
        def.shape = *Shape::MakeCircle(0.5f);
        auto* body = world->CreateBody(def);
        ATOM_CHECK(body != nullptr);

        RevoluteJointDef joint_def{};
        // 同一刚体自关节 → 拒绝。
        ATOM_CHECK(world->CreateRevoluteJoint(joint_def, body, body) == nullptr);

        // 不属于本世界的刚体 → 拒绝。
        auto world2 = backend.CreateWorld();
        auto* other = world2->CreateBody(def);
        ATOM_CHECK(other != nullptr);
        ATOM_CHECK(world->CreateRevoluteJoint(joint_def, body, other) == nullptr);
    }

    return 0;
}

#include <array>

#include <Physics/Contracts/IPhysicsBody.hpp>
#include <Physics/Contracts/IPhysicsWorld.hpp>
#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>
#include <Physics/Contracts/PhysicsTypes.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

namespace {

using atom::algo::Vec2;
using atom::physics::BodyDef;
using atom::physics::BodyType;
using atom::physics::IPhysicsBackend;
using atom::physics::Shape;

[[nodiscard]] auto MakeSquare(const float half) -> Shape {
    const std::array<Vec2, 4> sq = {Vec2{-half, -half}, Vec2{half, -half}, Vec2{half, half}, Vec2{-half, half}};
    return *Shape::MakePolygon(sq);
}

} // namespace

auto main() -> int {
    using namespace atom::physics;

    InternalPhysicsBackend backend{};

    // --- 自由落体：圆落到 static 地板上停住（不穿透、速度收敛）---
    {
        auto world = backend.CreateWorld();
        BodyDef floor_def{};
        floor_def.type = BodyType::Static;
        floor_def.shape = MakeSquare(5.0f);      // 宽 10 高 10，中心在 (0,0)
        floor_def.position = Vec2{0.0f, -6.0f}; // 顶面在 y=-1
        ATOM_CHECK(world->CreateBody(floor_def) != nullptr);

        BodyDef ball_def{};
        ball_def.type = BodyType::Dynamic;
        ball_def.shape = *Shape::MakeCircle(1.0f);
        ball_def.position = Vec2{0.0f, 5.0f};
        auto* ball = world->CreateBody(ball_def);
        ATOM_CHECK(ball != nullptr);

        for (int i = 0; i < 300; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }

        // 球停在地板顶（y=-1）+ 半径（1）= y≈0（含微小残余穿透）。
        ATOM_CHECK_NEAR(ball->GetPosition().GetY(), 0.0f, 0.15f);
        ATOM_CHECK_NEAR(ball->GetLinearVelocity().GetY(), 0.0f, 0.15f);
    }

    // --- 弹性反弹：高 restitution 的球落下后向上反弹 ---
    {
        auto world = backend.CreateWorld();
        BodyDef floor_def{};
        floor_def.type = BodyType::Static;
        floor_def.shape = MakeSquare(5.0f);
        floor_def.position = Vec2{0.0f, -6.0f};
        ATOM_CHECK(world->CreateBody(floor_def) != nullptr);

        BodyDef ball_def{};
        ball_def.type = BodyType::Dynamic;
        ball_def.shape = *Shape::MakeCircle(1.0f);
        ball_def.material.restitution = 0.9f;
        ball_def.position = Vec2{0.0f, 5.0f};
        auto* ball = world->CreateBody(ball_def);

        bool bounced = false;
        for (int i = 0; i < 300; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
            if (ball->GetLinearVelocity().GetY() > 0.5f) {
                bounced = true;
                break;
            }
        }
        ATOM_CHECK(bounced);
    }

    // --- 堆叠稳定：两个 dynamic 方块叠在地板上，收敛且不穿透 ---
    {
        auto world = backend.CreateWorld();
        BodyDef floor_def{};
        floor_def.type = BodyType::Static;
        floor_def.shape = MakeSquare(5.0f);
        floor_def.position = Vec2{0.0f, -6.0f};
        ATOM_CHECK(world->CreateBody(floor_def) != nullptr);

        BodyDef box_def{};
        box_def.type = BodyType::Dynamic;
        box_def.shape = MakeSquare(0.5f); // 半宽 0.5，边长 1

        box_def.position = Vec2{0.0f, -0.5f}; // 底面贴地板（y=-1+0.5=-0.5 为中心）
        auto* box1 = world->CreateBody(box_def);
        box_def.position = Vec2{0.0f, 0.5f}; // 堆在 box1 上
        auto* box2 = world->CreateBody(box_def);
        ATOM_CHECK(box1 != nullptr && box2 != nullptr);

        for (int i = 0; i < 400; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }

        // 收敛：速度趋近 0，位置接近贴叠（box1 中心 y≈-0.5，box2 中心 y≈0.5）。
        ATOM_CHECK_NEAR(box1->GetLinearVelocity().GetY(), 0.0f, 0.2f);
        ATOM_CHECK_NEAR(box2->GetLinearVelocity().GetY(), 0.0f, 0.2f);
        ATOM_CHECK_NEAR(box1->GetPosition().GetY(), -0.5f, 0.15f);
        ATOM_CHECK_NEAR(box2->GetPosition().GetY(), 0.5f, 0.15f);
        // 不穿透地板：box1 底面不低于地板顶面太多。
        ATOM_CHECK(box1->GetPosition().GetY() > -0.65f);
    }

    // --- 水平推动 + 摩擦减速：地面上的球被推动后因摩擦减速 ---
    {
        auto world = backend.CreateWorld();
        BodyDef floor_def{};
        floor_def.type = BodyType::Static;
        floor_def.shape = MakeSquare(20.0f);
        floor_def.position = Vec2{0.0f, -21.0f}; // 顶面 y=-1
        floor_def.material.friction = 0.5f;
        ATOM_CHECK(world->CreateBody(floor_def) != nullptr);

        BodyDef ball_def{};
        ball_def.type = BodyType::Dynamic;
        ball_def.shape = *Shape::MakeCircle(1.0f);
        ball_def.material.friction = 0.5f;
        ball_def.position = Vec2{0.0f, 0.0f};
        auto* ball = world->CreateBody(ball_def);

        // 先让它落稳
        for (int i = 0; i < 120; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }
        // 给水平冲量
        ball->ApplyLinearImpulse(Vec2{3.0f, 0.0f}, ball->GetPosition());
        world->Step(1.0f / 60.0f, 8, 3);
        const float vx_after_push = ball->GetLinearVelocity().GetX();
        ATOM_CHECK(vx_after_push > 0.0f); // 冲量生效
        // 后续摩擦应让它减速
        for (int i = 0; i < 120; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }
        ATOM_CHECK(ball->GetLinearVelocity().GetX() < vx_after_push);
    }

    return 0;
}

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file PhysicsIntegrationTest.cpp
 * @brief 物理模块整体 review 的大集成测试：跨 Shapes/Dynamics/Collision/Solver/
 *        PhysicsWorld/Backend 做场景级冒烟验证。
 *
 * 覆盖：确定性回放、金字塔堆叠稳定、混合场景（地面/滚动球/单摆/铰链链/抛体）
 * 能量有界、碰撞事件计数、长时间运行不发散。
 */

#include <array>
#include <cmath>
#include <vector>

#include <Backend/Contracts/Physics/IPhysicsBody.hpp>
#include <Backend/Contracts/Physics/IPhysicsJoint.hpp>
#include <Backend/Contracts/Physics/IPhysicsWorld.hpp>
#include <Backend/Physics/Internal/InternalPhysicsBackend.hpp>
#include <Backend/Contracts/Physics/PhysicsTypes.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

namespace {

using atom::algo::Vec2;
using atom::physics::BodyDef;
using atom::physics::BodyType;
using atom::physics::IPhysicsBody;
using atom::physics::IPhysicsWorld;
using atom::physics::InternalPhysicsBackend;
using atom::physics::RevoluteJointDef;
using atom::physics::Shape;

[[nodiscard]] auto IsFinite(const Vec2& v) -> bool {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY());
}

[[nodiscard]] auto MakeSquare(const float half) -> Shape {
    const std::array<Vec2, 4> sq = {Vec2{-half, -half}, Vec2{half, -half}, Vec2{half, half}, Vec2{-half, half}};
    return *Shape::MakePolygon(sq);
}

// 构建一个共享的复杂场景：地面 + 6 方块金字塔 + 1 滚动球 + 1 单摆 + 1 双体铰链链。
// 返回创建的所有动态体（经 out 出参），创建任一失败返回 false。
[[nodiscard]] auto BuildScene(IPhysicsWorld& world, std::vector<IPhysicsBody*>& out) -> bool {
    out.clear();

    // 地面（static）
    BodyDef floor{};
    floor.type = BodyType::Static;
    floor.shape = MakeSquare(12.0f);
    floor.position = Vec2{0.0f, -13.0f}; // 顶面 y=-1
    if (world.CreateBody(floor) == nullptr) {
        return false;
    }

    // 金字塔（3 行，6 个 dynamic 方块）
    BodyDef box{};
    box.type = BodyType::Dynamic;
    box.shape = MakeSquare(0.5f);
    const std::array<Vec2, 6> positions = {
        Vec2{-1.0f, -0.5f}, Vec2{0.0f, -0.5f}, Vec2{1.0f, -0.5f}, // 底层
        Vec2{-0.5f, 0.5f}, Vec2{0.5f, 0.5f},                       // 中层
        Vec2{0.0f, 1.5f},                                          // 顶层
    };
    for (const Vec2& p : positions) {
        box.position = p;
        IPhysicsBody* b = world.CreateBody(box);
        if (b == nullptr) {
            return false;
        }
        out.push_back(b);
    }

    // 滚动球（带摩擦，水平推动）
    BodyDef ball{};
    ball.type = BodyType::Dynamic;
    ball.shape = *Shape::MakeCircle(0.6f);
    ball.material.friction = 0.4f;
    ball.position = Vec2{4.0f, 2.0f};
    IPhysicsBody* roller = world.CreateBody(ball);
    if (roller == nullptr) {
        return false;
    }
    roller->ApplyLinearImpulse(Vec2{2.5f, 0.0f}, roller->GetPosition());
    out.push_back(roller);

    // 单摆（静态锚点 + 球，collide_connected=false）
    BodyDef pivot{};
    pivot.type = BodyType::Static;
    pivot.shape = *Shape::MakeCircle(0.1f);
    pivot.position = Vec2{-4.0f, 3.0f};
    IPhysicsBody* pivot_body = world.CreateBody(pivot);
    BodyDef bob{};
    bob.type = BodyType::Dynamic;
    bob.shape = *Shape::MakeCircle(0.4f);
    bob.position = Vec2{-4.0f, 2.0f};
    IPhysicsBody* bob_body = world.CreateBody(bob);
    if (pivot_body == nullptr || bob_body == nullptr) {
        return false;
    }
    RevoluteJointDef pendulum{};
    pendulum.local_anchor_a = Vec2{0.0f, 0.0f};
    pendulum.local_anchor_b = Vec2{0.0f, 0.4f};
    pendulum.collide_connected = false;
    if (world.CreateRevoluteJoint(pendulum, pivot_body, bob_body) == nullptr) {
        return false;
    }
    out.push_back(bob_body);

    // 双体铰链链
    BodyDef link{};
    link.type = BodyType::Dynamic;
    link.shape = MakeSquare(0.35f);
    link.position = Vec2{6.0f, 2.0f};
    IPhysicsBody* link_a = world.CreateBody(link);
    link.position = Vec2{6.7f, 2.0f};
    IPhysicsBody* link_b = world.CreateBody(link);
    if (link_a == nullptr || link_b == nullptr) {
        return false;
    }
    RevoluteJointDef chain{};
    chain.local_anchor_a = Vec2{0.35f, 0.0f};
    chain.local_anchor_b = Vec2{-0.35f, 0.0f};
    chain.collide_connected = false;
    if (world.CreateRevoluteJoint(chain, link_a, link_b) == nullptr) {
        return false;
    }
    out.push_back(link_a);
    out.push_back(link_b);

    return true;
}

struct CountListener final : atom::physics::IContactListener {
        int begins = 0;
        int ends = 0;
        auto BeginContact(atom::physics::IPhysicsBody*, atom::physics::IPhysicsBody*) -> void override {
            ++begins;
        }
        auto EndContact(atom::physics::IPhysicsBody*, atom::physics::IPhysicsBody*) -> void override {
            ++ends;
        }
};

} // namespace

auto main() -> int {
    using namespace atom::physics;

    InternalPhysicsBackend backend{};

    // --- 确定性回放：两个世界同场景同输入，600 步后逐位一致 ---
    {
        auto world_a = backend.CreateWorld();
        auto world_b = backend.CreateWorld();
        std::vector<IPhysicsBody*> bodies_a{};
        std::vector<IPhysicsBody*> bodies_b{};
        ATOM_CHECK(BuildScene(*world_a, bodies_a));
        ATOM_CHECK(BuildScene(*world_b, bodies_b));
        ATOM_CHECK(bodies_a.size() == bodies_b.size());

        for (int i = 0; i < 600; ++i) {
            world_a->Step(1.0f / 60.0f, 8, 3);
            world_b->Step(1.0f / 60.0f, 8, 3);
        }
        for (std::size_t i = 0; i < bodies_a.size(); ++i) {
            ATOM_CHECK(bodies_a[i]->GetPosition() == bodies_b[i]->GetPosition());
            ATOM_CHECK(bodies_a[i]->GetLinearVelocity() == bodies_b[i]->GetLinearVelocity());
            ATOM_CHECK(bodies_a[i]->GetAngle() == bodies_b[i]->GetAngle());
        }
    }

    // --- 金字塔堆叠：600 步后稳定、有界、不沉入地面 ---
    {
        auto world = backend.CreateWorld();
        std::vector<IPhysicsBody*> bodies{};
        ATOM_CHECK(BuildScene(*world, bodies));

        for (int i = 0; i < 600; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }
        for (IPhysicsBody* body : bodies) {
            ATOM_CHECK(IsFinite(body->GetPosition()));
            // 不沉穿地面（顶面 y=-1），不飞出场景。
            ATOM_CHECK(body->GetPosition().GetY() > -1.5f);
            ATOM_CHECK(std::abs(body->GetPosition().GetX()) < 14.0f);
        }
    }

    // --- 混合场景能量有界 + 碰撞事件计数 ---
    {
        auto world = backend.CreateWorld();
        CountListener listener{};
        world->SetContactListener(&listener);
        std::vector<IPhysicsBody*> bodies{};
        ATOM_CHECK(BuildScene(*world, bodies));

        for (int i = 0; i < 400; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }
        for (IPhysicsBody* body : bodies) {
            ATOM_CHECK(IsFinite(body->GetPosition()));
            ATOM_CHECK(IsFinite(body->GetLinearVelocity()));
            ATOM_CHECK(std::abs(body->GetPosition().GetX()) < 20.0f);
            ATOM_CHECK(std::abs(body->GetPosition().GetY()) < 20.0f);
        }
        ATOM_CHECK(listener.begins > 0); // 复杂场景必有接触发生
    }

    // --- 长时间运行：1500 步不发散、无 NaN ---
    {
        auto world = backend.CreateWorld();
        std::vector<IPhysicsBody*> bodies{};
        ATOM_CHECK(BuildScene(*world, bodies));

        for (int i = 0; i < 1500; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }
        for (IPhysicsBody* body : bodies) {
            ATOM_CHECK(IsFinite(body->GetPosition()));
            ATOM_CHECK(IsFinite(body->GetLinearVelocity()));
        }
    }

    return 0;
}

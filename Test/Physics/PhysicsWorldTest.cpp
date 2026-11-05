#include <Backend/Contracts/Physics/IPhysicsBody.hpp>
#include <Backend/Contracts/Physics/IPhysicsWorld.hpp>
#include <Backend/Physics/Internal/InternalPhysicsBackend.hpp>
#include <Backend/Contracts/Physics/PhysicsTypes.hpp>
#include <Physics/PhysicsWorld/FixedStepper.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <array>
#include <limits>

namespace {

using atom::algo::Vec2;
using atom::physics::BodyDef;
using atom::physics::BodyType;
using atom::physics::InternalPhysicsBackend;
using atom::physics::IPhysicsWorld;
using atom::physics::Shape;

struct PairListener final : atom::physics::IContactListener {
        int begin_count = 0;
        int end_count = 0;
        auto BeginContact(atom::physics::IPhysicsBody*, atom::physics::IPhysicsBody*) -> void override {
            ++begin_count;
        }
        auto EndContact(atom::physics::IPhysicsBody*, atom::physics::IPhysicsBody*) -> void override {
            ++end_count;
        }
};

struct DestroyingListener final : atom::physics::IContactListener {
        IPhysicsWorld* world = nullptr;
        atom::physics::IPhysicsBody* victim = nullptr;
        int begin_count = 0;

        auto BeginContact(atom::physics::IPhysicsBody*, atom::physics::IPhysicsBody*) -> void override {
            ++begin_count;
            world->DestroyBody(victim);
        }
};

struct ClearingListener final : atom::physics::IContactListener {
        IPhysicsWorld* world = nullptr;
        int begin_count = 0;

        auto BeginContact(atom::physics::IPhysicsBody*, atom::physics::IPhysicsBody*) -> void override {
            ++begin_count;
            world->SetContactListener(nullptr);
        }
};

[[nodiscard]] auto MakeSquare(const float half) -> Shape {
    const std::array<Vec2, 4> sq = {Vec2{-half, -half}, Vec2{half, -half}, Vec2{half, half}, Vec2{-half, half}};
    return *Shape::MakePolygon(sq);
}

} // namespace

auto main() -> int {
    using namespace atom::physics;

    InternalPhysicsBackend backend{};

    // --- FixedStepper：累加切分 + max_substeps 钳制 ---
    {
        auto world = backend.CreateWorld();
        BodyDef def{};
        def.type = BodyType::Dynamic;
        def.shape = *Shape::MakeCircle(0.5f);
        ATOM_CHECK(world->CreateBody(def) != nullptr);

        FixedStepper stepper{1.0f / 60.0f, 8, 3, 5};
        ATOM_CHECK(stepper.Advance(*world, 0.0f) == 0);
        ATOM_CHECK(stepper.Advance(*world, 0.01f) == 0); // 不足一帧，只累计
        ATOM_CHECK(stepper.Advance(*world, 0.01f) == 1); // 累计 0.02 ≥ 1/60，切 1 步

        FixedStepper clamped{1.0f / 60.0f, 8, 3, 5};
        ATOM_CHECK(clamped.Advance(*world, 0.5f) == 5); // 0.5s 远超 5 步，被钳制
        ATOM_CHECK(clamped.Accumulator() <= clamped.FixedDeltaTime());
    }

    // --- 碰撞事件配对：运动学体穿过静态体，Begin/End 各触发一次 ---
    {
        auto world = backend.CreateWorld();
        PairListener listener{};
        world->SetContactListener(&listener);

        BodyDef kin{};
        kin.type = BodyType::Kinematic;
        kin.shape = MakeSquare(0.5f);
        kin.position = Vec2{0.0f, 0.0f};
        auto* mover = world->CreateBody(kin);
        ATOM_CHECK(mover != nullptr);
        mover->SetLinearVelocity(Vec2{10.0f, 0.0f});

        BodyDef stat{};
        stat.type = BodyType::Static;
        stat.shape = MakeSquare(0.5f);
        stat.position = Vec2{5.0f, 0.0f};
        ATOM_CHECK(world->CreateBody(stat) != nullptr);

        for (int i = 0; i < 60; ++i) { // 6s × 10m/s = 60m，远超穿过 5m 处
            world->Step(1.0f / 60.0f, 8, 3);
        }
        ATOM_CHECK(listener.begin_count == 1);
        ATOM_CHECK(listener.end_count == 1);
    }

    // --- 休眠：球落稳后入睡，速度清零且位置不再漂移 ---
    {
        auto world = backend.CreateWorld();
        BodyDef floor{};
        floor.type = BodyType::Static;
        floor.shape = MakeSquare(5.0f);
        floor.position = Vec2{0.0f, -6.0f};
        ATOM_CHECK(world->CreateBody(floor) != nullptr);

        BodyDef ball{};
        ball.type = BodyType::Dynamic;
        ball.shape = *Shape::MakeCircle(1.0f);
        ball.position = Vec2{0.0f, 5.0f};
        auto* b = world->CreateBody(ball);
        ATOM_CHECK(b != nullptr);

        for (int i = 0; i < 200; ++i) { // 3.3s，足够落稳 + 越过 0.5s 休眠阈值
            world->Step(1.0f / 60.0f, 8, 3);
        }
        ATOM_CHECK(!b->IsAwake());
        ATOM_CHECK(b->GetLinearVelocity() == Vec2::Zero());

        const Vec2 pos_before = b->GetPosition();
        for (int i = 0; i < 60; ++i) { // 休眠后继续跑，位置不变
            world->Step(1.0f / 60.0f, 8, 3);
        }
        ATOM_CHECK(b->GetPosition() == pos_before);

        // 施加冲量唤醒并产生运动
        b->ApplyLinearImpulse(Vec2{2.0f, 0.0f}, b->GetPosition());
        ATOM_CHECK(b->IsAwake());
        ATOM_CHECK(b->GetLinearVelocity().GetX() > 0.0f);
    }

    // --- 堆叠休眠：两个方块叠在地板上都入睡（无睡眠乒乓）---
    {
        auto world = backend.CreateWorld();
        BodyDef floor{};
        floor.type = BodyType::Static;
        floor.shape = MakeSquare(5.0f);
        floor.position = Vec2{0.0f, -6.0f};
        ATOM_CHECK(world->CreateBody(floor) != nullptr);

        BodyDef box{};
        box.type = BodyType::Dynamic;
        box.shape = MakeSquare(0.5f);
        box.position = Vec2{0.0f, -0.5f};
        auto* box1 = world->CreateBody(box);
        box.position = Vec2{0.0f, 0.5f};
        auto* box2 = world->CreateBody(box);
        ATOM_CHECK(box1 != nullptr && box2 != nullptr);

        for (int i = 0; i < 400; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }
        ATOM_CHECK(!box1->IsAwake());
        ATOM_CHECK(!box2->IsAwake());
    }

    // --- 关闭休眠：全部刚体保持清醒 ---
    {
        auto world = backend.CreateWorld();
        BodyDef ball{};
        ball.type = BodyType::Dynamic;
        ball.shape = *Shape::MakeCircle(0.5f);
        auto* b = world->CreateBody(ball);
        ATOM_CHECK(b != nullptr);

        world->SetSleepingEnabled(false);
        for (int i = 0; i < 200; ++i) {
            world->Step(1.0f / 60.0f, 8, 3);
        }
        ATOM_CHECK(b->IsAwake());
    }

    // --- 两个分离的休眠体共享宽相 cell 不能伪造接触事件 ---
    {
        auto world = backend.CreateWorld();
        PairListener listener{};
        world->SetContactListener(&listener);

        BodyDef body{};
        body.type = BodyType::Dynamic;
        body.shape = *Shape::MakeCircle(0.1f);
        body.position = Vec2{0.0f, 0.0f};
        auto* a = world->CreateBody(body);
        body.position = Vec2{0.9f, 0.0f}; // 与 a 分离，但 AABB 都落入 cell 0。
        auto* b = world->CreateBody(body);
        ATOM_CHECK(a != nullptr && b != nullptr);
        a->SetAwake(false);
        b->SetAwake(false);

        world->Step(1.0f / 60.0f, 8, 3);
        ATOM_CHECK(listener.begin_count == 0);
    }

    // --- 回调中销毁刚体延后到本步结束，求解器不解引用悬垂 contact ---
    {
        auto world = backend.CreateWorld();
        DestroyingListener listener{};
        listener.world = world.get();
        world->SetContactListener(&listener);

        BodyDef body{};
        body.type = BodyType::Dynamic;
        body.shape = *Shape::MakeCircle(1.0f);
        auto* a = world->CreateBody(body);
        body.position = Vec2{0.0f, 1.0f};
        auto* b = world->CreateBody(body);
        ATOM_CHECK(a != nullptr && b != nullptr);
        listener.victim = b;

        world->Step(1.0f / 60.0f, 8, 3);
        ATOM_CHECK(listener.begin_count == 1);
        // 下一步验证 deferred destroy 已完成且 world 仍可安全推进。
        world->Step(1.0f / 60.0f, 8, 3);
    }

    // --- 回调可替换 listener，不使同一批已开始的事件派发失效 ---
    {
        auto world = backend.CreateWorld();
        ClearingListener listener{};
        listener.world = world.get();
        world->SetContactListener(&listener);

        BodyDef body{};
        body.type = BodyType::Dynamic;
        body.shape = *Shape::MakeCircle(1.0f);
        ATOM_CHECK(world->CreateBody(body) != nullptr);
        body.position = Vec2{0.0f, 0.5f};
        ATOM_CHECK(world->CreateBody(body) != nullptr);
        body.position = Vec2{0.0f, -0.5f};
        ATOM_CHECK(world->CreateBody(body) != nullptr);

        world->Step(1.0f / 60.0f, 8, 3);
        ATOM_CHECK(listener.begin_count == 3);
    }

    // --- 外部 NaN 输入被拒绝，不污染 world 状态 ---
    {
        auto world = backend.CreateWorld();
        BodyDef invalid{};
        invalid.type = BodyType::Dynamic;
        invalid.shape = *Shape::MakeCircle(0.5f);
        invalid.position = Vec2{std::numeric_limits<float>::quiet_NaN(), 0.0f};
        ATOM_CHECK(world->CreateBody(invalid) == nullptr);

        BodyDef valid{};
        valid.type = BodyType::Dynamic;
        valid.shape = *Shape::MakeCircle(0.5f);
        auto* body = world->CreateBody(valid);
        ATOM_CHECK(body != nullptr);
        const Vec2 old_position = body->GetPosition();
        body->SetTransform(Vec2{std::numeric_limits<float>::quiet_NaN(), 0.0f}, 0.0f);
        ATOM_CHECK(body->GetPosition() == old_position);
    }

    return 0;
}

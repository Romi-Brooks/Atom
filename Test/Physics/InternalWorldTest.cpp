#include <Physics/Contracts/IPhysicsBody.hpp>
#include <Physics/Contracts/IPhysicsWorld.hpp>
#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>
#include <Physics/Contracts/PhysicsTypes.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

namespace {

struct CountingListener final : atom::physics::IContactListener {
        int begin_count = 0;
        auto BeginContact(atom::physics::IPhysicsBody*, atom::physics::IPhysicsBody*) -> void override {
            ++begin_count;
        }
};

} // namespace

auto main() -> int {
    using namespace atom::physics;
    using atom::algo::Vec2;

    InternalPhysicsBackend backend{};
    auto world = backend.CreateWorld();
    ATOM_CHECK(world != nullptr);

    // --- 自由落体：dynamic 圆一步后下落 ---
    {
        BodyDef def{};
        def.type = BodyType::Dynamic;
        def.shape = *Shape::MakeCircle(1.0f);
        def.material.density = 1.0f;
        auto* body = world->CreateBody(def);
        ATOM_CHECK(body != nullptr);
        ATOM_CHECK(body->GetType() == BodyType::Dynamic);
        ATOM_CHECK(body->GetPosition() == Vec2::Zero());

        world->Step(1.0f / 60.0f, 8, 3);
        ATOM_CHECK(body->GetPosition().GetY() < 0.0f); // 受重力下落
        ATOM_CHECK(body->GetLinearVelocity().GetY() < 0.0f);
    }

    // --- 确定性回放：两个独立世界相同输入，结果逐位一致 ---
    {
        auto world_a = backend.CreateWorld();
        auto world_b = backend.CreateWorld();
        BodyDef def{};
        def.type = BodyType::Dynamic;
        def.shape = *Shape::MakeCircle(0.5f);
        def.position = Vec2{1.0f, 2.0f};
        auto* a = world_a->CreateBody(def);
        auto* b = world_b->CreateBody(def);
        ATOM_CHECK(a != nullptr && b != nullptr);

        for (int i = 0; i < 120; ++i) {
            world_a->Step(1.0f / 60.0f, 8, 3);
            world_b->Step(1.0f / 60.0f, 8, 3);
        }
        ATOM_CHECK(a->GetPosition() == b->GetPosition());
        ATOM_CHECK(a->GetLinearVelocity() == b->GetLinearVelocity());
    }

    // --- dynamic 零质量（退化形状）被拒绝 ---
    {
        BodyDef def{};
        def.type = BodyType::Dynamic;
        def.shape = *Shape::MakeSegment(Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}); // 面积 0
        auto* body = world->CreateBody(def);
        ATOM_CHECK(body == nullptr);
    }

    // --- static 允许零质量（不参与积分）---
    {
        BodyDef def{};
        def.type = BodyType::Static;
        def.shape = *Shape::MakeSegment(Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f});
        auto* body = world->CreateBody(def);
        ATOM_CHECK(body != nullptr);
    }

    // --- ApplyForce 产生加速度；DestroyBody 后不再 Step ---
    {
        BodyDef def{};
        def.type = BodyType::Dynamic;
        def.shape = *Shape::MakeCircle(0.5f);
        auto* body = world->CreateBody(def);
        ATOM_CHECK(body != nullptr);

        body->ApplyForce(Vec2{10.0f, 0.0f}, body->GetPosition());
        world->Step(1.0f / 60.0f, 8, 3);
        ATOM_CHECK(body->GetLinearVelocity().GetX() > 0.0f);

        world->DestroyBody(body);
    }

    // --- 窄相位集成：重叠的两个圆触发 BeginContact ---
    {
        auto w = backend.CreateWorld();
        CountingListener listener{};
        w->SetContactListener(&listener);

        BodyDef def{};
        def.type = BodyType::Dynamic;
        def.shape = *Shape::MakeCircle(1.0f);
        def.position = Vec2{0.0f, 0.0f};
        ATOM_CHECK(w->CreateBody(def) != nullptr);

        def.position = Vec2{0.0f, 1.0f}; // 与上一个圆重叠（圆心距 1 < 2）
        ATOM_CHECK(w->CreateBody(def) != nullptr);

        w->Step(1.0f / 60.0f, 8, 3);
        ATOM_CHECK(listener.begin_count >= 1);
    }

    return 0;
}

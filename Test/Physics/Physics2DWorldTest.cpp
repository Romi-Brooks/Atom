/**
 * @file           : Physics2DWorldTest.cpp
 * @brief          : T3.1b - the ECS <-> physics bridge.
 *
 * The bridge is judged on five things, each of which is a bug when it is wrong:
 *
 *   1. units: a body authored in pixels falls in a way that looks right on screen, which
 *      only holds if the metre/pixel and y-axis conversion happens in exactly one place;
 *   2. authority: a dynamic body's transform is written *by* physics, not fought over by
 *      gameplay every frame;
 *   3. no leaks: destroying the entity destroys the body, because otherwise an invisible
 *      collider keeps interacting with the world;
 *   4. sensors: a trigger reports contacts and never shoves anything;
 *   5. contacts: named begin/persist/end, with both entities resolvable, so a caller can
 *      turn them into its own event type without a side table.
 */

#include <Physics/Integration/Physics2DWorld.hpp>

#include <ECS/Components.hpp>
#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <vector>

namespace {

using namespace atom;
using atom::ecs::Entity;
using atom::ecs::PhysicsBody2D;
using atom::ecs::PhysicsBodyKind;
using atom::ecs::Transform2D;
using atom::physics::InternalPhysicsBackend;
using atom::physics::integration::ContactEvent2D;
using atom::physics::integration::ContactPhase;
using atom::physics::integration::Physics2DWorld;

// 100 px per metre: a 100x100 px crate is one metre square, which keeps the numbers easy
// to check by hand.
constexpr float kPixelsPerMeter = 100.0F;

auto MakeBody(const PhysicsBodyKind kind, const algo::Vec2 position, const algo::Vec2 half_extent,
              const bool is_sensor = false) -> std::pair<Entity, PhysicsBody2D> {
    PhysicsBody2D shell{};
    shell.kind = kind;
    shell.half_extent = half_extent;
    shell.is_sensor = is_sensor;
    return {Entity{}, shell};
}

auto AddBody(ecs::EntityWorld& entities, const PhysicsBodyKind kind, const algo::Vec2 position,
             const algo::Vec2 half_extent, const bool is_sensor = false) -> Entity {
    const auto entity = entities.CreateEntity();
    PhysicsBody2D shell{};
    shell.kind = kind;
    shell.half_extent = half_extent;
    shell.is_sensor = is_sensor;
    if (!entities.Emplace<Transform2D>(entity, Transform2D{.position = position}).has_value())
        return {};
    if (!entities.Emplace<PhysicsBody2D>(entity, shell).has_value())
        return {};
    return entity;
}

auto HasEvent(const std::vector<ContactEvent2D>& events, const ContactPhase phase, const Entity a, const Entity b) -> bool {
    for (const auto& event : events) {
        if (event.phase != phase)
            continue;
        if ((event.a == a && event.b == b) || (event.a == b && event.b == a))
            return true;
    }
    return false;
}

} // namespace

auto main() -> int {
    InternalPhysicsBackend backend{};

    // --- 1. A dynamic body falls and lands on a static floor --------------
    //
    // Floor top at y = 600 px; the box is 100x100 px, so a resting centre is y = 500.
    // The world is y-down, so "falling" means the y value *increases*.
    {
        auto created = ecs::EntityWorld::Create();
        ATOM_CHECK(created.has_value());
        auto& entities = **created;

        const auto world = Physics2DWorld::Create(backend, kPixelsPerMeter);
        ATOM_CHECK(world != nullptr);

        // Static floor: 20 m wide, 1 m thick, top edge at 6 m (600 px).
        const auto floor = AddBody(entities, PhysicsBodyKind::Static, {0.0F, 650.0F}, {10.0F, 0.5F});
        ATOM_CHECK(floor.IsValid());
        const auto box = AddBody(entities, PhysicsBodyKind::Dynamic, {0.0F, 0.0F}, {0.5F, 0.5F});
        ATOM_CHECK(box.IsValid());

        float lowest = 0.0F;
        for (int step = 0; step < 240; ++step) {
            auto events = world->Step(entities, 1.0F / 60.0F);
            ATOM_CHECK(events.has_value());
            lowest = std::max(lowest, entities.TryGet<Transform2D>(box)->position.GetY());
        }

        // Fell a long way (gravity is real: ~9.81 m/s^2 -> 981 px/s^2) and stopped on the
        // floor instead of passing through it. 500 px is the resting centre; a little
        // penetration is allowed, falling through is not.
        ATOM_CHECK(lowest > 400.0F);
        ATOM_CHECK(lowest < 600.0F);
        ATOM_CHECK(world->BodyCount() == 2);
    }

    // --- 2. Destroying the entity destroys the body ----------------------
    {
        auto created = ecs::EntityWorld::Create();
        ATOM_CHECK(created.has_value());
        auto& entities = **created;

        const auto world = Physics2DWorld::Create(backend, kPixelsPerMeter);
        ATOM_CHECK(world != nullptr);

        const auto box = AddBody(entities, PhysicsBodyKind::Dynamic, {0.0F, 0.0F}, {0.5F, 0.5F});
        ATOM_CHECK(box.IsValid());
        auto events = world->Step(entities, 1.0F / 60.0F);
        ATOM_CHECK(events.has_value());
        ATOM_CHECK(world->BodyCount() == 1);

        ATOM_CHECK(entities.Destroy(box).has_value());
        events = world->Step(entities, 1.0F / 60.0F);
        ATOM_CHECK(events.has_value());
        // The body is gone: a destroyed entity must not leave a collider nobody can see.
        ATOM_CHECK(world->BodyCount() == 0);
    }

    // --- 3. A sensor reports, never resolves -----------------------------
    {
        auto created = ecs::EntityWorld::Create();
        ATOM_CHECK(created.has_value());
        auto& entities = **created;

        const auto world = Physics2DWorld::Create(backend, kPixelsPerMeter);
        ATOM_CHECK(world != nullptr);
        world->SetGravity({0.0F, 0.0F}); // horizontal pass-through, keeps the maths obvious

        const auto trigger = AddBody(entities, PhysicsBodyKind::Static, {300.0F, 0.0F}, {1.0F, 1.0F}, true);
        ATOM_CHECK(trigger.IsValid());
        // Kinematic, not dynamic: a dynamic body's transform is written *by* physics, so
        // teleporting it from the test would be overwritten each step. A kinematic body is
        // the documented way for gameplay to drive a body, and it still collides with the
        // sensor - which is all this section needs.
        const auto traveller = AddBody(entities, PhysicsBodyKind::Kinematic, {0.0F, 0.0F}, {0.5F, 0.5F});
        ATOM_CHECK(traveller.IsValid());

        // Walk it across the trigger one step at a time.
        bool saw_begin = false;
        bool saw_persist = false;
        bool saw_sensor_flag = false;
        for (int step = 0; step < 120; ++step) {
            auto* transform = entities.TryGet<Transform2D>(traveller);
            transform->position = {transform->position.GetX() + 10.0F, 0.0F};

            auto events = world->Step(entities, 1.0F / 60.0F);
            ATOM_CHECK(events.has_value());
            if (HasEvent(*events, ContactPhase::Begin, trigger, traveller)) {
                saw_begin = true;
                for (const auto& event : *events) {
                    if (event.phase == ContactPhase::Begin && event.sensor)
                        saw_sensor_flag = true;
                }
            }
            if (HasEvent(*events, ContactPhase::Persist, trigger, traveller))
                saw_persist = true;
        }

        ATOM_CHECK(saw_begin);
        ATOM_CHECK(saw_persist);
        // The event says "this contact involves a sensor", so a consumer does not have to
        // query the components to tell a trigger from a wall.
        ATOM_CHECK(saw_sensor_flag);
    }

    // --- 4. Persist is reported for a resting body -----------------------
    {
        auto created = ecs::EntityWorld::Create();
        ATOM_CHECK(created.has_value());
        auto& entities = **created;

        const auto world = Physics2DWorld::Create(backend, kPixelsPerMeter);
        ATOM_CHECK(world != nullptr);

        const auto floor = AddBody(entities, PhysicsBodyKind::Static, {0.0F, 100.0F}, {10.0F, 0.5F});
        const auto box = AddBody(entities, PhysicsBodyKind::Dynamic, {0.0F, 0.0F}, {0.5F, 0.5F});
        ATOM_CHECK(floor.IsValid());
        ATOM_CHECK(box.IsValid());

        std::size_t persist_count = 0;
        std::size_t begin_count = 0;
        for (int step = 0; step < 240; ++step) {
            auto events = world->Step(entities, 1.0F / 60.0F);
            ATOM_CHECK(events.has_value());
            for (const auto& event : *events) {
                if (event.phase == ContactPhase::Persist &&
                    ((event.a == floor && event.b == box) || (event.a == box && event.b == floor)))
                    ++persist_count;
                if (event.phase == ContactPhase::Begin)
                    ++begin_count;
            }
        }

        // One begin, many persists: the state "it is standing on the floor" is what a
        // gameplay system actually needs, and it must not be re-derived from begin/end.
        ATOM_CHECK(begin_count == 1);
        ATOM_CHECK(persist_count > 30);
    }

    // --- 5. Unit conversion is one way round -----------------------------
    {
        const auto world = Physics2DWorld::Create(backend, kPixelsPerMeter);
        ATOM_CHECK(world != nullptr);
        // y flips: 200 px below the origin is -2 m in physics space.
        ATOM_CHECK_NEAR(world->ToMeters({200.0F, 200.0F}).GetY(), -2.0F);
        ATOM_CHECK_NEAR(world->ToMeters({200.0F, 200.0F}).GetX(), 2.0F);
        ATOM_CHECK_NEAR(world->ToPixels({2.0F, -2.0F}).GetY(), 200.0F);
        // And the pair round-trips.
        const auto round_tripped = world->ToPixels(world->ToMeters({37.0F, -21.0F}));
        ATOM_CHECK_NEAR(round_tripped.GetX(), 37.0F);
        ATOM_CHECK_NEAR(round_tripped.GetY(), -21.0F);
    }

    // --- 6. A bad step delta is refused, not silently applied -------------
    {
        auto created = ecs::EntityWorld::Create();
        ATOM_CHECK(created.has_value());
        auto& entities = **created;
        const auto world = Physics2DWorld::Create(backend, kPixelsPerMeter);
        ATOM_CHECK(world != nullptr);

        ATOM_CHECK(!world->Step(entities, 0.0F).has_value());
        ATOM_CHECK(!world->Step(entities, -0.016F).has_value());
    }

    return 0;
}

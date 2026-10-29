/**
 * @file           : SensorAndUserDataTest.cpp
 * @brief          : T3.1a - sensors report contacts and never resolve them; bodies
 *                    carry a stable owner id; contacts persist.
 *
 * These three additions to the physics contract exist because the ECS integration layer
 * cannot be written without them:
 *
 *   - `is_sensor` is the difference between "the player is inside the exit trigger" and
 *     "the exit trigger shoves the player". Without it every trigger needs a second,
 *     hand-rolled overlap test outside the physics world.
 *   - `user_data` is how a contact callback finds its owner. A `void*` would invite a read
 *     after the owner is gone; a number plus a lookup table owned by the integration layer
 *     cannot dangle.
 *   - `PersistContact` is "still touching": a floor contact is persistent by nature, and
 *     with only Begin/End every listener would have to keep its own pair set.
 */

#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>
#include <Physics/Contracts/IPhysicsBackend.hpp>
#include <Physics/Contracts/IPhysicsBody.hpp>
#include <Physics/Contracts/IPhysicsWorld.hpp>
#include <Physics/Shapes/Shape.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <algorithm>
#include <string_view>
#include <cstdint>
#include <vector>

namespace {

using namespace atom::physics;
using atom::algo::Vec2;

// Records the contact lifecycle in the order the world reports it.
class ContactLog final : public IContactListener {
    public:
        struct Event {
                const char* kind;
                std::uint64_t a;
                std::uint64_t b;
        };

        auto BeginContact(IPhysicsBody* a, IPhysicsBody* b) -> void override {
            events.push_back({"begin", a->GetUserData(), b->GetUserData()});
        }
        auto PersistContact(IPhysicsBody* a, IPhysicsBody* b) -> void override {
            events.push_back({"persist", a->GetUserData(), b->GetUserData()});
        }
        auto EndContact(IPhysicsBody* a, IPhysicsBody* b) -> void override {
            events.push_back({"end", a->GetUserData(), b->GetUserData()});
        }

        [[nodiscard]] auto Count(const char* kind) const -> std::size_t {
            return static_cast<std::size_t>(
                std::count_if(events.begin(), events.end(), [kind](const Event& e) { return std::string_view{e.kind} == kind; }));
        }

        std::vector<Event> events{};
};



} // namespace

auto main() -> int {
    InternalPhysicsBackend backend{};
    auto created_world = backend.CreateWorld();
    ATOM_CHECK(created_world != nullptr);
    IPhysicsWorld* world = created_world.get();
    world->SetGravity({0.0F, 0.0F}); // horizontal only: keeps the arithmetic obvious

    ContactLog log{};
    world->SetContactListener(&log);

    // --- 1. user_data and is_sensor survive creation ----------------------
    {
        BodyDef static_def{};
        static const Vec2 kUnitSquare[] = {{-1.0F, -1.0F}, {1.0F, -1.0F}, {1.0F, 1.0F}, {-1.0F, 1.0F}};
        static_def.shape = *Shape::MakePolygon(kUnitSquare);
        static_def.type = BodyType::Static;
        static_def.position = {0.0F, 0.0F};
        static_def.user_data = 0xABCDEF01ULL;
        auto* wall = world->CreateBody(static_def);
        ATOM_CHECK(wall != nullptr);
        ATOM_CHECK(wall->GetUserData() == 0xABCDEF01ULL);
        ATOM_CHECK(!wall->IsSensor());

        BodyDef sensor_def{};
        sensor_def.shape = *Shape::MakeCircle(0.5F);
        sensor_def.type = BodyType::Static;
        sensor_def.position = {10.0F, 0.0F};
        sensor_def.is_sensor = true;
        sensor_def.user_data = 7;
        auto* trigger = world->CreateBody(sensor_def);
        ATOM_CHECK(trigger != nullptr);
        ATOM_CHECK(trigger->IsSensor());
        ATOM_CHECK(trigger->GetUserData() == 7);

        // Clean up: an obstructing box left in the world would silently stop the next
        // section's traveller, which is exactly the bug this cleanup prevents.
        world->DestroyBody(wall);
        world->DestroyBody(trigger);
    }

    // --- 2. A sensor reports the touch and does not stop the body ---------
    //
    // The dynamic body starts left of the sensor and is pushed right through it. With a
    // real collider it would stop at the surface; with a sensor it must pass through while
    // producing begin/persist/end.
    {
        world->SetContactListener(&log);
        log.events.clear();

        BodyDef traveller_def{};
        traveller_def.shape = *Shape::MakeCircle(0.5F);
        traveller_def.type = BodyType::Dynamic;
        traveller_def.material.density = 1.0F;
        traveller_def.position = {-3.0F, 0.0F};
        traveller_def.user_data = 1;
        auto* traveller = world->CreateBody(traveller_def);
        ATOM_CHECK(traveller != nullptr);
        traveller->SetLinearVelocity({4.0F, 0.0F});

        BodyDef gate_def{};
        gate_def.shape = *Shape::MakeCircle(0.75F);
        gate_def.type = BodyType::Static;
        gate_def.position = {0.0F, 0.0F};
        gate_def.is_sensor = true;
        gate_def.user_data = 2;
        auto* gate = world->CreateBody(gate_def);
        ATOM_CHECK(gate != nullptr);

        // Step across the sensor and out the far side.
        float max_x = traveller->GetPosition().GetX();
        for (int step = 0; step < 180; ++step) {
            world->Step(1.0F / 60.0F, 8, 3);
            max_x = std::max(max_x, traveller->GetPosition().GetX());
        }

        // It kept moving: a sensor applies no impulse.
        ATOM_CHECK(max_x > 5.0F);
        // It reported the crossing: begin, at least one persist, end.
        ATOM_CHECK(log.Count("begin") >= 1);
        ATOM_CHECK(log.Count("persist") >= 1);
        ATOM_CHECK(log.Count("end") >= 1);

        // Ordering contract: begin before persist before end for the same pair.
        std::size_t first_begin = log.events.size();
        std::size_t first_persist = log.events.size();
        std::size_t first_end = log.events.size();
        for (std::size_t i = 0; i < log.events.size(); ++i) {
            const std::string_view kind{log.events[i].kind};
            if (kind == "begin" && first_begin == log.events.size())
                first_begin = i;
            if (kind == "persist" && first_persist == log.events.size())
                first_persist = i;
            if (kind == "end" && first_end == log.events.size())
                first_end = i;
        }
        ATOM_CHECK(first_begin <= first_persist);
        ATOM_CHECK(first_persist < first_end);

        // The pair is identified by user data, which is what lets the integration layer
        // map it back to entities without a side table of pointers.
        bool saw_expected_pair = false;
        for (const auto& event : log.events) {
            const bool has_one = event.a == 1 || event.b == 1;
            const bool has_gate = event.a == 2 || event.b == 2;
            saw_expected_pair = saw_expected_pair || (has_one && has_gate);
        }
        ATOM_CHECK(saw_expected_pair);

        world->DestroyBody(traveller);
        world->DestroyBody(gate);
    }

    // --- 3. A real collider still stops the same traveller ----------------
    //
    // The control case: the sensor branch must not have disabled collision response.
    {
        log.events.clear();

        BodyDef wall_def{};
        static const Vec2 kWallShape[] = {{-1.0F, -3.0F}, {1.0F, -3.0F}, {1.0F, 3.0F}, {-1.0F, 3.0F}};
        wall_def.shape = *Shape::MakePolygon(kWallShape);
        wall_def.type = BodyType::Static;
        wall_def.position = {0.0F, 0.0F};
        wall_def.user_data = 3;
        auto* wall = world->CreateBody(wall_def);
        ATOM_CHECK(wall != nullptr);

        BodyDef mover_def{};
        mover_def.shape = *Shape::MakeCircle(0.5F);
        mover_def.type = BodyType::Dynamic;
        mover_def.material.density = 1.0F;
        mover_def.position = {-3.0F, 0.0F};
        mover_def.user_data = 4;
        auto* mover = world->CreateBody(mover_def);
        ATOM_CHECK(mover != nullptr);
        mover->SetLinearVelocity({6.0F, 0.0F});

        for (int step = 0; step < 120; ++step)
            world->Step(1.0F / 60.0F, 8, 3);

        // Stopped on the left face instead of passing through (radius 0.5 + half width 1).
        ATOM_CHECK(mover->GetPosition().GetX() < -1.0F);
        ATOM_CHECK(log.Count("begin") >= 1);

        world->DestroyBody(mover);
        world->DestroyBody(wall);
    }

    // --- 4. Resting on a floor keeps reporting persist --------------------
    {
        log.events.clear();
        world->SetGravity({0.0F, -9.8F});

        BodyDef floor_def{};
        static const Vec2 kFloorShape[] = {{-5.0F, -0.5F}, {5.0F, -0.5F}, {5.0F, 0.5F}, {-5.0F, 0.5F}};
        floor_def.shape = *Shape::MakePolygon(kFloorShape);
        floor_def.type = BodyType::Static;
        floor_def.position = {0.0F, -1.0F};
        floor_def.user_data = 5;
        auto* floor = world->CreateBody(floor_def);
        ATOM_CHECK(floor != nullptr);

        BodyDef box_def{};
        static const Vec2 kBoxShape[] = {{-0.5F, -0.5F}, {0.5F, -0.5F}, {0.5F, 0.5F}, {-0.5F, 0.5F}};
        box_def.shape = *Shape::MakePolygon(kBoxShape);
        box_def.type = BodyType::Dynamic;
        box_def.material.density = 1.0F;
        box_def.position = {0.0F, 2.0F};
        box_def.user_data = 6;
        auto* box = world->CreateBody(box_def);
        ATOM_CHECK(box != nullptr);

        for (int step = 0; step < 240; ++step)
            world->Step(1.0F / 60.0F, 8, 3);

        // It landed (not still falling) and kept reporting while resting: that is the case
        // Begin/End alone cannot describe.
        // Rests on the floor top (y = -0.5) instead of falling through it; the exact resting
        // height is solver-dependent, so the assertion is "did not penetrate".
        ATOM_CHECK(box->GetPosition().GetY() > -0.6F);
        ATOM_CHECK(log.Count("persist") >= 30);

        world->DestroyBody(box);
        world->DestroyBody(floor);
        world->SetGravity({0.0F, 0.0F});
    }

    return 0;
}

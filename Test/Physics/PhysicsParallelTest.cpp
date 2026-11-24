// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <array>
#include <cstddef>
#include <vector>

#include <Algorithm/Vector/Vec2.hpp>
#include <Physics/Contracts/IPhysicsBody.hpp>
#include <Physics/Contracts/IPhysicsWorld.hpp>
#include <Physics/Contracts/PhysicsTypes.hpp>
#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>
#include <Concurrency/JobSystem.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Test/Support/TestHelpers.hpp>

namespace {

using atom::algo::Vec2;
using atom::physics::BodyDef;
using atom::physics::BodyType;
using atom::physics::IPhysicsBody;
using atom::physics::IPhysicsWorld;
using atom::physics::Shape;

[[nodiscard]] auto BuildDenseScene(IPhysicsWorld& world, std::vector<IPhysicsBody*>& bodies) -> bool {
    const std::array<Vec2, 4> vertices = {
        Vec2{-0.45f, -0.45f}, Vec2{0.45f, -0.45f}, Vec2{0.45f, 0.45f}, Vec2{-0.45f, 0.45f}};
    const auto shape = Shape::MakePolygon(vertices);
    if (!shape) {
        return false;
    }

    world.SetGravity(Vec2::Zero());
    world.SetSleepingEnabled(false);
    bodies.clear();
    constexpr std::size_t kColumns = 16;
    constexpr std::size_t kBodies = kColumns * kColumns;
    for (std::size_t index = 0; index < kBodies; ++index) {
        BodyDef body{};
        body.type = BodyType::Dynamic;
        body.shape = *shape;
        body.material.friction = 0.4f;
        body.position = Vec2{static_cast<float>(index % kColumns) * 0.85f,
                             static_cast<float>(index / kColumns) * 0.85f};
        IPhysicsBody* created = world.CreateBody(body);
        if (created == nullptr) {
            return false;
        }
        bodies.push_back(created);
    }
    return true;
}

auto Step(IPhysicsWorld& world) -> void {
    for (int step = 0; step < 20; ++step) {
        world.Step(1.0f / 60.0f, 8, 3);
    }
}

} // namespace

auto main() -> int {
    atom::physics::InternalPhysicsBackend backend{};

    // First run without worker threads, then rerun the identical scene with
    // range-partitioned narrow phase. Exact equality proves completion order
    // cannot leak into solver input order.
    auto serial_world = backend.CreateWorld();
    std::vector<IPhysicsBody*> serial_bodies{};
    ATOM_CHECK(BuildDenseScene(*serial_world, serial_bodies));
    Step(*serial_world);

    auto& jobs = atom::job::JobSystem::GetInstance();
    jobs.Configure(4);
    ATOM_CHECK(jobs.Start());

    auto parallel_world = backend.CreateWorld();
    std::vector<IPhysicsBody*> parallel_bodies{};
    ATOM_CHECK(BuildDenseScene(*parallel_world, parallel_bodies));
    Step(*parallel_world);
    jobs.Shutdown();

    ATOM_CHECK(serial_bodies.size() == parallel_bodies.size());
    for (std::size_t index = 0; index < serial_bodies.size(); ++index) {
        ATOM_CHECK(serial_bodies[index]->GetPosition() == parallel_bodies[index]->GetPosition());
        ATOM_CHECK(serial_bodies[index]->GetLinearVelocity() == parallel_bodies[index]->GetLinearVelocity());
        ATOM_CHECK(serial_bodies[index]->GetAngle() == parallel_bodies[index]->GetAngle());
    }
    return 0;
}

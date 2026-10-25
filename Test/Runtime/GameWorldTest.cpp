/**
 * @file GameWorldTest.cpp
 * @brief Unit coverage for deterministic GameWorld orchestration.
 */

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include <ECS/Components.hpp>
#include <Runtime/GameWorld.hpp>
#include <Test/Support/TestHelpers.hpp>

namespace {

using atom::Result;
using atom::ecs::Entity;
using atom::ecs::Position;
using atom::runtime::FrameContext;
using atom::runtime::GameEvent;
using atom::runtime::GameWorld;
using atom::runtime::SystemDescriptor;
using atom::runtime::SystemPhase;

struct PhaseRecorder final {
        std::vector<std::string> calls{};
        Entity first_spawned{};
        bool destroy_queued = false;
};

auto AddRecorderSystem(GameWorld& world, const SystemPhase phase, const char* name, PhaseRecorder& recorder) -> int {
    const auto result = world.GetScheduler().Add(
        phase,
        SystemDescriptor{std::string{name}, [&recorder, name](GameWorld& callback_world, const FrameContext& context) {
                             recorder.calls.emplace_back(name);
                             if (context.phase == SystemPhase::Input && std::string_view{name} == "input-a") {
                                 callback_world.GetCommands().QueueCreate(
                                     [&recorder](atom::ecs::EntityWorld& entities, const Entity entity) {
                                         recorder.first_spawned = entity;
                                         return entities.Emplace<Position>(entity, Position{});
                                     });
                             }
                             if (context.phase == SystemPhase::FixedUpdate && recorder.first_spawned.IsValid() &&
                                 !recorder.destroy_queued) {
                                 callback_world.GetCommands().QueueDestroy(recorder.first_spawned);
                                 recorder.destroy_queued = true;
                             }
                             return atom::Ok();
                         }});
    return result.has_value() ? 0 : 1;
}

} // namespace

auto main() -> int {
    auto created = GameWorld::Create();
    ATOM_CHECK(created.has_value());
    auto& world = **created;

    // Systems run in phase order and insertion order. Commands are visible to
    // the following phase, never to a still-running system.
    PhaseRecorder recorder{};
    ATOM_CHECK(AddRecorderSystem(world, SystemPhase::Input, "input-a", recorder) == 0);
    ATOM_CHECK(AddRecorderSystem(world, SystemPhase::Input, "input-b", recorder) == 0);
    ATOM_CHECK(AddRecorderSystem(world, SystemPhase::FixedUpdate, "fixed", recorder) == 0);
    ATOM_CHECK(AddRecorderSystem(world, SystemPhase::Update, "update", recorder) == 0);
    ATOM_CHECK(AddRecorderSystem(world, SystemPhase::LateUpdate, "late", recorder) == 0);
    ATOM_CHECK(world.RunFrame(1.0f / 60.0f, 2, 1.0f / 120.0f).has_value());
    ATOM_CHECK((recorder.calls == std::vector<std::string>{"input-a", "input-b", "fixed", "fixed", "update", "late"}));
    ATOM_CHECK(recorder.first_spawned.IsValid());
    ATOM_CHECK(!world.GetEntities().IsAlive(recorder.first_spawned));
    ATOM_CHECK(world.GetFrameIndex() == 1);

    // Registration rejects ambiguous order and invalid descriptors.
    ATOM_CHECK(!world.GetScheduler()
                    .Add(SystemPhase::Update,
                         SystemDescriptor{"update", [](GameWorld&, const FrameContext&) { return atom::Ok(); }})
                    .has_value());

    // A system cannot modify the schedule that is currently being iterated.
    auto guard_created = GameWorld::Create();
    ATOM_CHECK(guard_created.has_value());
    auto& guard_world = **guard_created;
    bool rejected_while_running = false;
    ATOM_CHECK(
        guard_world.GetScheduler()
            .Add(SystemPhase::Update,
                 SystemDescriptor{"mutation-guard",
                                  [&rejected_while_running](GameWorld& callback_world, const FrameContext&) {
                                      const auto add = callback_world.GetScheduler().Add(
                                          SystemPhase::Update,
                                          SystemDescriptor{"must-not-register",
                                                           [](GameWorld&, const FrameContext&) { return atom::Ok(); }});
                                      rejected_while_running = !add.has_value();
                                      return atom::Ok();
                                  }})
            .has_value());
    ATOM_CHECK(guard_world.RunFrame(0.0f, 0, 0.0f).has_value());
    ATOM_CHECK(rejected_while_running);
    ATOM_CHECK(
        !world.GetScheduler()
             .Add(SystemPhase::Update, SystemDescriptor{"", [](GameWorld&, const FrameContext&) { return atom::Ok(); }})
             .has_value());

    // Events preserve FIFO order and Drain establishes the next collection.
    const auto source = world.GetEntities().CreateEntity();
    const auto target = world.GetEntities().CreateEntity();
    world.GetEvents().Enqueue(GameEvent{7, source, target});
    world.GetEvents().Enqueue(GameEvent{9, target, source});
    const auto events = world.GetEvents().Drain();
    ATOM_CHECK(events.size() == 2);
    ATOM_CHECK(events[0].type == 7 && events[0].source == source && events[0].target == target);
    ATOM_CHECK(events[1].type == 9 && events[1].source == target && events[1].target == source);
    ATOM_CHECK(world.GetEvents().Count() == 0);

    // Invalid timing data cannot create platform-specific simulation behaviour.
    ATOM_CHECK(!world.RunFrame(std::nanf(""), 0, 0.0f).has_value());
    ATOM_CHECK(!world.RunFrame(0.0f, 0, -1.0f).has_value());

    return 0;
}

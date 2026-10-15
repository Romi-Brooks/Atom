/**
 * @file           : EntityWorldTest.cpp
 * @brief          : EntityWorld contract tests.
 *
 * The cases here are chosen deliberately: each one is a misuse whose measured
 * behaviour with the raw storage backend is either silent state corruption or a
 * wild-pointer crash in a release build. They pass only because EntityWorld
 * validates before touching the backend, which is the whole reason the facade
 * exists. If someone later "simplifies" the facade by delegating straight to the
 * backend, these tests are what fails.
**/

#include <ECS/Components.hpp>
#include <ECS/EntityWorld.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <algorithm>
#include <span>

namespace {

using atom::ErrorCode;
using atom::ecs::Active;
using atom::ecs::Entity;
using atom::ecs::EntityWorld;
using atom::ecs::Health;
using atom::ecs::Name;
using atom::ecs::Position;
using atom::ecs::Velocity;

auto MakeWorld() -> std::unique_ptr<EntityWorld> {
    auto created = EntityWorld::Create();
    if (!created.has_value()) {
        std::cerr << "Could not create the world: " << created.error().Message() << '\n';
        return nullptr;
    }
    return std::move(*created);
}

// Shared state for the integration visitor below.
struct IntegrateState final {
        float delta = 1.0f;
        int visited = 0;
        Entity last_entity{};
        std::vector<std::uint32_t> order{};
};

// A visitor in the shape the engine's systems use: a free function, so the loop
// body can be compiled against the real component types without the public header
// naming the storage backend.
struct Integrate final {
        static auto Run(void* context, const Entity entity, const std::span<Position> positions,
                        const std::span<Velocity> velocities) -> void {
            auto& state = *static_cast<IntegrateState*>(context);
            ++state.visited;
            state.last_entity = entity;
            state.order.push_back(entity.Index());
            positions[0].value = positions[0].value + velocities[0].value * state.delta;
        }
};

} // namespace

auto main() -> int {
    auto world = MakeWorld();
    ATOM_CHECK(world != nullptr);

    // --- 1. Entity lifecycle and handle validity --------------------------
    {
        const auto first = world->CreateEntity();
        ATOM_CHECK(first.IsValid());
        ATOM_CHECK(world->IsAlive(first));
        ATOM_CHECK(world->AliveCount() == 1);

        const auto second = world->CreateEntity();
        ATOM_CHECK(second.IsValid());
        ATOM_CHECK(second.Index() != first.Index());
        ATOM_CHECK(world->AliveCount() == 2);

        ATOM_CHECK(world->Destroy(second).has_value());
        ATOM_CHECK(!world->IsAlive(second));
        ATOM_CHECK(world->AliveCount() == 1);

        // Destroying a dead handle must be an error, not a silent success: the
        // backend's own destroy() reports this only through an assertion, and in
        // a release build that means the call appears to work.
        const auto again = world->Destroy(second);
        ATOM_CHECK(!again.has_value());
        ATOM_CHECK(again.error().Code() == ErrorCode::EcsEntityNotAlive);
        ATOM_CHECK(again.error().GetModule() == atom::Module::Ecs);

        // A default-constructed handle is rejected as invalid, distinctly from
        // "used to be alive".
        const Entity never{};
        ATOM_CHECK(!never.IsValid());
        ATOM_CHECK(!world->IsAlive(never));
        const auto rejected = world->Destroy(never);
        ATOM_CHECK(!rejected.has_value());
        ATOM_CHECK(rejected.error().Code() == ErrorCode::EcsInvalidEntity);
    }

    // --- 2. Slot recycling invalidates the old handle ---------------------
    {
        auto world = MakeWorld();
        const auto original = world->CreateEntity();
        const auto index = original.Index();

        ATOM_CHECK(world->Destroy(original).has_value());
        // Free slots include the reserved index 0, so this is an inequality
        // rather than an exact count: what matters is that the slot came back.
        ATOM_CHECK(world->FreeSlotCount() >= 1);
        ATOM_CHECK(world->AliveCount() == 0);

        const auto recycled = world->CreateEntity();
        ATOM_CHECK(recycled.IsValid());
        // The index is reused, the generation must differ.
        ATOM_CHECK(recycled.Index() == index);
        ATOM_CHECK(recycled.Generation() != original.Generation());
        ATOM_CHECK(recycled != original);

        // The stale handle must be dead, and every operation on it refused.
        ATOM_CHECK(!world->IsAlive(original));
        ATOM_CHECK(!world->Has<Position>(original));
        ATOM_CHECK(world->TryGet<Position>(original) == nullptr);
        ATOM_CHECK(!world->Get<Position>(original).has_value());
        ATOM_CHECK(!world->Emplace<Position>(original, Position{}).has_value());

        // The new handle works.
        ATOM_CHECK(world->IsAlive(recycled));
        ATOM_CHECK(world->Emplace<Position>(recycled, Position{}).has_value());
    }

    // --- 3. Component insert / read / replace ----------------------------
    {
        auto world = MakeWorld();
        const auto entity = world->CreateEntity();

        ATOM_CHECK(!world->Has<Position>(entity));
        ATOM_CHECK(world->TryGet<Position>(entity) == nullptr);

        // Reading a component the entity does not own is undefined behaviour in
        // the raw backend, and a crash in a release build. It must be an error.
        const auto missing = world->Get<Position>(entity);
        ATOM_CHECK(!missing.has_value());
        ATOM_CHECK(missing.error().Code() == ErrorCode::EcsComponentMissing);

        const auto inserted = world->Emplace<Position>(entity, Position{{3.0f, 4.0f}});
        ATOM_CHECK(inserted.has_value());
        ATOM_CHECK(world->Has<Position>(entity));
        ATOM_CHECK(world->CountOf<Position>() == 1);

        auto* position = world->TryGet<Position>(entity);
        ATOM_CHECK(position != nullptr);
        ATOM_CHECK(position->value.GetX() == 3.0f);
        ATOM_CHECK(position->value.GetY() == 4.0f);

        // Inserting twice is undefined behaviour in the raw backend and silently
        // overwrites the value in a release build. It must be an error.
        const auto duplicate = world->Emplace<Position>(entity, Position{{9.0f, 9.0f}});
        ATOM_CHECK(!duplicate.has_value());
        ATOM_CHECK(duplicate.error().Code() == ErrorCode::EcsComponentAlreadyPresent);
        // ...and the stored value must be untouched by the rejected call.
        ATOM_CHECK(world->TryGet<Position>(entity)->value.GetX() == 3.0f);

        // Replacing is explicit, and separate.
        ATOM_CHECK(world->EmplaceOrReplace<Position>(entity, Position{{1.0f, 2.0f}}).has_value());
        ATOM_CHECK(world->TryGet<Position>(entity)->value.GetX() == 1.0f);
        ATOM_CHECK(world->CountOf<Position>() == 1);

        // Patch requires presence.
        ATOM_CHECK(world->Patch<Position>(entity, Position{{7.0f, 8.0f}}).has_value());
        ATOM_CHECK(world->TryGet<Position>(entity)->value.GetY() == 8.0f);
        const auto other = world->CreateEntity();
        const auto patch_missing = world->Patch<Position>(other, Position{});
        ATOM_CHECK(!patch_missing.has_value());
        ATOM_CHECK(patch_missing.error().Code() == ErrorCode::EcsComponentMissing);

        // Removal is idempotent.
        ATOM_CHECK(world->Remove<Position>(entity).has_value());
        ATOM_CHECK(!world->Has<Position>(entity));
        ATOM_CHECK(world->Remove<Position>(entity).has_value());
        ATOM_CHECK(world->CountOf<Position>() == 0);
    }

    // --- 4. Multiple components per entity are independent ----------------
    {
        auto world = MakeWorld();
        const auto entity = world->CreateEntity();

        ATOM_CHECK(world->Emplace<Position>(entity, Position{{1.0f, 1.0f}}).has_value());
        ATOM_CHECK(world->Emplace<Velocity>(entity, Velocity{{2.0f, 0.0f}}).has_value());
        ATOM_CHECK(world->Emplace<Health>(entity, Health{10, 20}).has_value());
        ATOM_CHECK(world->Emplace<Name>(entity, Name{"player"}).has_value());

        ATOM_CHECK(world->Has<Position>(entity));
        ATOM_CHECK(world->Has<Velocity>(entity));
        ATOM_CHECK(world->Has<Health>(entity));
        ATOM_CHECK(world->Has<Name>(entity));
        ATOM_CHECK(!world->Has<atom::ecs::Rotation>(entity));

        ATOM_CHECK(world->CountOf<Position>() == 1);
        ATOM_CHECK(world->CountOf<Velocity>() == 1);
        ATOM_CHECK(world->CountOf<Health>() == 1);
        ATOM_CHECK(world->CountOf<Name>() == 1);

        // Removing one leaves the others alone.
        ATOM_CHECK(world->Remove<Health>(entity).has_value());
        ATOM_CHECK(!world->Has<Health>(entity));
        ATOM_CHECK(world->Has<Position>(entity));
        ATOM_CHECK(world->Has<Name>(entity));
        ATOM_CHECK(world->TryGet<Name>(entity)->value == "player");
    }

    // --- 5. Tag components carry presence, not a value --------------------
    {
        auto world = MakeWorld();
        const auto entity = world->CreateEntity();

        ATOM_CHECK(!world->Has<Active>(entity));
        ATOM_CHECK(world->Emplace<Active>(entity).has_value());
        ATOM_CHECK(world->Has<Active>(entity));
        ATOM_CHECK(world->CountOf<Active>() == 1);

        // Present with no value: a second insert is still a duplicate.
        const auto duplicate = world->Emplace<Active>(entity);
        ATOM_CHECK(!duplicate.has_value());
        ATOM_CHECK(duplicate.error().Code() == ErrorCode::EcsComponentAlreadyPresent);

        ATOM_CHECK(world->Remove<Active>(entity).has_value());
        ATOM_CHECK(!world->Has<Active>(entity));

        // The API knows a tag has no value to patch.
        ATOM_CHECK(EntityWorld::IsTag<Active>());
        ATOM_CHECK(!EntityWorld::IsTag<Position>());
    }

    // --- 6. Destroying an entity removes every component ------------------
    {
        auto world = MakeWorld();
        const auto entity = world->CreateEntity();
        ATOM_CHECK(world->Emplace<Position>(entity, Position{}).has_value());
        ATOM_CHECK(world->Emplace<Velocity>(entity, Velocity{}).has_value());
        ATOM_CHECK(world->CountOf<Position>() == 1);
        ATOM_CHECK(world->CountOf<Velocity>() == 1);

        ATOM_CHECK(world->Destroy(entity).has_value());
        ATOM_CHECK(world->AliveCount() == 0);
        ATOM_CHECK(world->CountOf<Position>() == 0);
        ATOM_CHECK(world->CountOf<Velocity>() == 0);

        // And the handle is refused afterwards.
        ATOM_CHECK(!world->Emplace<Position>(entity, Position{}).has_value());
        ATOM_CHECK(world->TryGet<Position>(entity) == nullptr);
    }

    // --- 7. Component pools do not leak between entities ------------------
    {
        auto world = MakeWorld();
        std::vector<Entity> entities{};
        for (int index = 0; index < 64; ++index) {
            const auto entity = world->CreateEntity();
            ATOM_CHECK(entity.IsValid());
            if (index % 2 == 0)
                ATOM_CHECK(world->Emplace<Position>(entity, Position{{static_cast<float>(index), 0.0f}}).has_value());
            entities.push_back(entity);
        }
        ATOM_CHECK(world->AliveCount() == 64);
        ATOM_CHECK(world->CountOf<Position>() == 32);

        // The value of each populated entity is its own.
        for (std::size_t index = 0; index < entities.size(); index += 2) {
            auto* position = world->TryGet<Position>(entities[index]);
            ATOM_CHECK(position != nullptr);
            ATOM_CHECK(position->value.GetX() == static_cast<float>(index));
        }

        // Destroy half, then recreate: the pool must follow the live entities.
        for (std::size_t index = 0; index < entities.size(); index += 2)
            ATOM_CHECK(world->Destroy(entities[index]).has_value());
        ATOM_CHECK(world->CountOf<Position>() == 0);
        ATOM_CHECK(world->AliveCount() == 32);
    }

    // --- 8. Collection is deterministic and filtered ----------------------
    {
        auto world = MakeWorld();
        std::vector<Entity> both{};
        std::vector<Entity> position_only{};

        for (int index = 0; index < 8; ++index) {
            const auto entity = world->CreateEntity();
            ATOM_CHECK(world->Emplace<Position>(entity, Position{{static_cast<float>(index), 0.0f}}).has_value());
            if (index % 2 == 0) {
                ATOM_CHECK(world->Emplace<Velocity>(entity, Velocity{}).has_value());
                both.push_back(entity);
            } else {
                position_only.push_back(entity);
            }
        }

        // Only entities owning *all* listed components are collected.
        const auto with_both = world->With<Position, Velocity>();
        ATOM_CHECK(with_both.size() == both.size());
        // Ascending index order, which is what makes stepping reproducible.
        ATOM_CHECK(std::is_sorted(with_both.begin(), with_both.end(), [](const Entity l, const Entity r) {
            return l.Index() < r.Index();
        }));
        for (const auto entity : both) {
            ATOM_CHECK(std::find(with_both.begin(), with_both.end(), entity) != with_both.end());
        }

        // A single-component collection returns every owner.
        const auto with_position = world->With<Position>();
        ATOM_CHECK(with_position.size() == both.size() + position_only.size());
        for (const auto entity : position_only) {
            ATOM_CHECK(std::find(with_position.begin(), with_position.end(), entity) != with_position.end());
        }

        // Removing the shared component drops the entity out of the pair query
        // without touching the single-component one.
        ATOM_CHECK(world->Remove<Velocity>(both.front()).has_value());
        ATOM_CHECK(world->With<Position, Velocity>().size() == both.size() - 1);
        ATOM_CHECK(world->With<Position>().size() == both.size() + position_only.size());
    }

    // --- 9. Typed iteration ------------------------------------------------
    {
        auto world = MakeWorld();

        auto moving_a = world->CreateEntity();
        auto moving_b = world->CreateEntity();
        auto still = world->CreateEntity();

        ATOM_CHECK(world->Emplace<Position>(moving_a, Position{{0.0f, 0.0f}}).has_value());
        ATOM_CHECK(world->Emplace<Velocity>(moving_a, Velocity{{1.0f, 0.0f}}).has_value());
        ATOM_CHECK(world->Emplace<Position>(moving_b, Position{{0.0f, 10.0f}}).has_value());
        ATOM_CHECK(world->Emplace<Velocity>(moving_b, Velocity{{2.0f, 0.0f}}).has_value());
        // No velocity: must not be visited by the integration loop.
        ATOM_CHECK(world->Emplace<Position>(still, Position{{99.0f, 99.0f}}).has_value());

        IntegrateState state{};
        state.delta = 0.5f;
        const auto visited = world->ForEach<Position, Velocity>({&Integrate::Run, &state});
        ATOM_CHECK(visited.has_value());
        ATOM_CHECK(state.visited == 2);
        ATOM_CHECK(state.order.size() == 2);
        // Exactly the two entities owning both components were visited. ForEach does
        // not sort: it walks the pools in place, which is what keeps it cheap. The
        // order is stable for a given sequence of world operations, which is what a
        // fixed-step simulation needs; With() is the sorted view.
        ATOM_CHECK(std::find(state.order.begin(), state.order.end(), moving_a.Index()) != state.order.end());
        ATOM_CHECK(std::find(state.order.begin(), state.order.end(), moving_b.Index()) != state.order.end());
        ATOM_CHECK(std::find(state.order.begin(), state.order.end(), still.Index()) == state.order.end());

        // The writes the body made are the stored values.
        ATOM_CHECK(world->TryGet<Position>(moving_a)->value.GetX() == 0.5f);
        ATOM_CHECK(world->TryGet<Position>(moving_b)->value.GetX() == 1.0f);
        ATOM_CHECK(world->TryGet<Position>(moving_b)->value.GetY() == 10.0f);
        // The entity without a Velocity was left alone.
        ATOM_CHECK(world->TryGet<Position>(still)->value.GetX() == 99.0f);

        // A null visitor is refused rather than doing nothing silently.
        const auto null_visit = world->ForEach<Position, Velocity>({nullptr, nullptr});
        ATOM_CHECK(!null_visit.has_value());

        // Empty result is not an error.
        const auto other = MakeWorld();
        IntegrateState empty_state{};
        const auto none = other->ForEach<Position, Velocity>({&Integrate::Run, &empty_state});
        ATOM_CHECK(none.has_value());
        ATOM_CHECK(empty_state.visited == 0);
    }

    // --- 10. Tags are checked per entity, not listed in the query ----------
    {
        auto world = MakeWorld();

        const auto active = world->CreateEntity();
        const auto inactive = world->CreateEntity();
        for (const auto entity : {active, inactive}) {
            ATOM_CHECK(world->Emplace<Position>(entity, Position{}).has_value());
            ATOM_CHECK(world->Emplace<Velocity>(entity, Velocity{{1.0f, 1.0f}}).has_value());
        }
        ATOM_CHECK(world->Emplace<Active>(active).has_value());

        // The backend omits empty types from its callback, so a tag cannot appear in
        // the visitor signature; the body checks it instead. This is the shape the
        // first physics loop uses to skip inactive entities.
        struct TagState final {
                int visited = 0;
        };
        struct CountingVisitor final {
                static auto Run(void* context, const Entity entity, const std::span<Position> positions,
                                const std::span<Velocity> velocities) -> void {
                    (void)entity;
                    (void)positions;
                    (void)velocities;
                    ++static_cast<TagState*>(context)->visited;
                }
        };

        TagState state{};
        const auto result = world->ForEach<Position, Velocity>({&CountingVisitor::Run, &state});
        ATOM_CHECK(result.has_value());
        // Both entities own Position and Velocity; the tag is not part of the query.
        ATOM_CHECK(state.visited == 2);

        // And the tag is what a system uses to filter inside the body.
        int active_count = 0;
        for (const auto entity : world->With<Position, Velocity>()) {
            if (world->Has<Active>(entity))
                ++active_count;
        }
        ATOM_CHECK(active_count == 1);
    }

    // --- 11. Validation must not depend on assertions being enabled --------
    {
        // The facade's checks are the first line of defence and must survive
        // NDEBUG, because the storage backend's own assertions do not. In a Debug
        // build the backend would abort on these misuses, so a bug here would be
        // masked; in a Release build it would be silent corruption or a crash.
        // Running the same expectations in both configurations is what proves the
        // facade is not leaning on the backend's assertions.
        auto world = MakeWorld();
        const auto original = world->CreateEntity();
        ATOM_CHECK(world->Emplace<Position>(original, Position{{1.0f, 1.0f}}).has_value());
        ATOM_CHECK(world->Destroy(original).has_value());

        const auto recycled = world->CreateEntity();
        ATOM_CHECK(recycled.Index() == original.Index());
        ATOM_CHECK(recycled.Generation() != original.Generation());

        // Every one of these is undefined behaviour or state corruption if it
        // reaches the backend unchecked.
        ATOM_CHECK(world->TryGet<Position>(original) == nullptr);
        ATOM_CHECK(!world->Get<Position>(original).has_value());
        ATOM_CHECK(!world->Has<Position>(original));
        ATOM_CHECK(!world->Emplace<Position>(original, Position{}).has_value());
        ATOM_CHECK(!world->Remove<Position>(original).has_value());
        ATOM_CHECK(!world->Destroy(original).has_value());

        // The live handle in the recycled slot is untouched by all of that.
        ATOM_CHECK(!world->Has<Position>(recycled));
        ATOM_CHECK(world->Emplace<Position>(recycled, Position{{2.0f, 2.0f}}).has_value());
        ATOM_CHECK(!world->Emplace<Position>(recycled, Position{{9.0f, 9.0f}}).has_value());
        ATOM_CHECK(world->TryGet<Position>(recycled)->value.GetX() == 2.0f);

#if defined(NDEBUG)
        // Recorded so a reviewer can tell which configuration produced a failure
        // in CI, where the Release build defines NDEBUG and the Debug build does
        // not.
        std::cout << "entity-world validation checked with NDEBUG defined\n";
#else
        std::cout << "entity-world validation checked with assertions enabled\n";
#endif
    }

    return 0;
}

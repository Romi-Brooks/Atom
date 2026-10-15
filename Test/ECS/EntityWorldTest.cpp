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

    return 0;
}

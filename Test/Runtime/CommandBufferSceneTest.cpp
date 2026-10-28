/**
 * @file           : CommandBufferSceneTest.cpp
 * @brief          : T1.1 - build 2D scene entities through the command buffer.
 *
 * T1.3's GameScreen creates its world contents from inside a phase, which means
 * every entity, and every component on it, must be constructible from a
 * SpawnSetup rather than only from a direct EntityWorld call. Component
 * *existence* is covered by Atom_ECS.ComponentVocabulary; this test covers the
 * spawn path and its failure semantics:
 *
 *   - a create command applies its setup with the real entity handle;
 *   - a setup that fails destroys the entity instead of leaving half a world;
 *   - destroys queued in the same batch are applied after the creates.
 */

#include <ECS/Components.hpp>
#include <Runtime/EntityCommandBuffer.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <string>

namespace {

using namespace atom;

// Finds the single entity carrying a given scene id, or an invalid handle.
auto FindBySceneId(ecs::EntityWorld& world, const std::uint32_t id) -> ecs::Entity {
    for (const auto entity : world.With<ecs::SceneEntityId>()) {
        if (const auto* existing = world.TryGet<ecs::SceneEntityId>(entity); existing != nullptr && existing->value == id)
            return entity;
    }
    return {};
}

} // namespace

auto main() -> int {
    auto created = ecs::EntityWorld::Create();
    ATOM_CHECK(created.has_value());
    auto& world = **created;

    runtime::EntityCommandBuffer commands{};

    // Make one long-lived entity so the destroy path below has a victim that was
    // not created in this batch.
    const auto victim = world.CreateEntity();
    ATOM_CHECK(world.Emplace<ecs::Transform2D>(victim).has_value());
    ATOM_CHECK(world.Emplace<ecs::RenderTransform2D>(victim).has_value());
    ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(victim, ecs::SpriteRenderer{.texture_uri = "res://victim.png"})
                    .has_value());
    const auto baseline = world.AliveCount();

    // --- 1. A sprite entity, a camera entity, and a failing spawn ---------
    commands.QueueCreate([](ecs::EntityWorld& target, const ecs::Entity entity) -> Result<void> {
        if (auto placed = target.Emplace<ecs::SceneEntityId>(entity, ecs::SceneEntityId{.value = 1}); !placed.has_value())
            return placed;
        if (auto placed = target.Emplace<ecs::Transform2D>(entity, ecs::Transform2D{.position = {48.0f, 96.0f}});
            !placed.has_value())
            return placed;
        if (auto placed = target.Emplace<ecs::RenderTransform2D>(entity); !placed.has_value())
            return placed;

        ecs::SpriteRenderer sprite{};
        sprite.texture_uri = "res://textures/player.png";
        sprite.source_size = {16.0f, 16.0f};
        sprite.pivot = {0.5f, 1.0f};
        sprite.layer = 4;
        sprite.sort_key = 2;
        sprite.flip_x = true;
        return target.Emplace<ecs::SpriteRenderer>(entity, sprite);
    });

    commands.QueueCreate([](ecs::EntityWorld& target, const ecs::Entity entity) -> Result<void> {
        if (auto placed = target.Emplace<ecs::SceneEntityId>(entity, ecs::SceneEntityId{.value = 2}); !placed.has_value())
            return placed;
        if (auto placed = target.Emplace<ecs::Transform2D>(entity, ecs::Transform2D{.position = {400.0f, 300.0f}});
            !placed.has_value())
            return placed;
        return target.Emplace<ecs::Camera2D>(entity, ecs::Camera2D{.zoom = 2.0f});
    });

    // A setup that fails halfway: the transform goes on, the second emplace is a
    // duplicate and fails. The command buffer must destroy the entity, not leave
    // a transform-only ghost behind.
    commands.QueueCreate([](ecs::EntityWorld& target, const ecs::Entity entity) -> Result<void> {
        if (auto placed = target.Emplace<ecs::Transform2D>(entity); !placed.has_value())
            return placed;
        return target.Emplace<ecs::Transform2D>(entity);
    });

    commands.QueueDestroy(victim);

    ATOM_CHECK(commands.Count() == 4);
    ATOM_CHECK(!commands.Flush(world).has_value());

    // --- 2. Failure semantics ---------------------------------------------
    //
    // The failing spawn must not survive; the two good spawns must. The victim
    // was queued for destroy after the creates, so it is gone as well.
    ATOM_CHECK(world.AliveCount() == baseline + 2 - 1);
    ATOM_CHECK(!world.IsAlive(victim));
    ATOM_CHECK(world.CountOf<ecs::Camera2D>() == 1);

    // --- 3. The created entities carry the components the setup asked for --
    {
        const auto player = FindBySceneId(world, 1);
        ATOM_CHECK(player.IsValid());
        ATOM_CHECK(world.IsAlive(player));

        const auto* transform = world.TryGet<ecs::Transform2D>(player);
        ATOM_CHECK(transform != nullptr);
        ATOM_CHECK(transform->position.GetX() == 48.0f && transform->position.GetY() == 96.0f);

        const auto* display = world.TryGet<ecs::RenderTransform2D>(player);
        ATOM_CHECK(display != nullptr);

        const auto* sprite = world.TryGet<ecs::SpriteRenderer>(player);
        ATOM_CHECK(sprite != nullptr);
        ATOM_CHECK(sprite->texture_uri == "res://textures/player.png");
        ATOM_CHECK(sprite->layer == 4 && sprite->sort_key == 2);
        ATOM_CHECK(sprite->flip_x);
        ATOM_CHECK(sprite->pivot.GetY() == 1.0f);

        const auto camera = FindBySceneId(world, 2);
        ATOM_CHECK(camera.IsValid());
        const auto* lens = world.TryGet<ecs::Camera2D>(camera);
        ATOM_CHECK(lens != nullptr && lens->zoom == 2.0f);
        ATOM_CHECK(world.Has<ecs::Transform2D>(camera));

        // The failing spawn left nothing: no entity owns a scene id beyond the two.
        ATOM_CHECK(world.With<ecs::SceneEntityId>().size() == 2);
    }

    // --- 4. Deferred destroy during a phase does not touch live iteration --
    //
    // Mirrors what GameWorld does at a phase boundary: queue, then flush once
    // iteration is finished.
    {
        const auto doomed = FindBySceneId(world, 1);
        runtime::EntityCommandBuffer deferred{};
        deferred.QueueDestroy(doomed);
        for (const auto entity : world.With<ecs::RenderTransform2D, ecs::SpriteRenderer>()) {
            // Still alive while iterating; the flush below is what removes it.
            ATOM_CHECK(world.IsAlive(entity));
        }
        ATOM_CHECK(deferred.Flush(world).has_value());
        ATOM_CHECK(!world.IsAlive(doomed));
        ATOM_CHECK(world.With<ecs::RenderTransform2D, ecs::SpriteRenderer>().empty());
    }

    return 0;
}

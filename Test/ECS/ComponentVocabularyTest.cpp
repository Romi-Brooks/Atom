/**
 * @file           : ComponentVocabularyTest.cpp
 * @brief          : T1.1 - the frozen first batch of 2D scene components.
 *
 * What this locks down:
 *   - the six component types satisfy the ECS component concepts and stay plain
 *     value data (nothing here can hold a resource handle or a backend pointer);
 *   - each of them can be emplaced, read, patched and removed through the public
 *     API;
 *   - the new query sets in EntityWorld.cpp are actually instantiated and return
 *     exactly the entities that own every listed component;
 *   - ForEach visits in ascending entity index and the span body can write.
 *
 * The set-instantiation assertions matter as much as the API ones: a query set
 * that is used but not instantiated fails at link time, and this test is the
 * only place that would catch it before a consumer does.
 *
 * Building entities through the EntityCommandBuffer is covered separately by
 * Atom_Runtime.CommandBufferScene, so this test can stay free of Runtime (and
 * therefore of any window/backend linkage).
 */

#include <ECS/Components.hpp>
#include <ECS/EntityWorld.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace atom::ecs;

// --- 1. Concept and shape guarantees ----------------------------------------

static_assert(Component<Transform2D>);
static_assert(ValueComponent<Transform2D>);
static_assert(Component<RenderTransform2D>);
static_assert(ValueComponent<SpriteRenderer>);
static_assert(ValueComponent<Camera2D>);
static_assert(ValueComponent<Lifetime>);
static_assert(ValueComponent<SceneEntityId>);

// Trivial copy types are what makes the relocatable pools cheap: no allocation,
// no copy-constructor side effects, no ownership to relocate.
static_assert(std::is_trivially_copyable_v<Transform2D>);
static_assert(std::is_trivially_copyable_v<RenderTransform2D>);
static_assert(std::is_trivially_copyable_v<Camera2D>);
static_assert(std::is_trivially_copyable_v<Lifetime>);
static_assert(std::is_trivially_copyable_v<SceneEntityId>);

// SpriteRenderer is the one component with an allocation (the asset URI). It is
// still movable, which is all the pools require.
static_assert(std::is_move_constructible_v<SpriteRenderer>);

// The vocabulary must not smuggle a handle or a backend type into ECS (A0.1
// rules R2-R4). These assertions name the fields, so an edit that swaps a value
// for a pointer fails here rather than at a consumer's include.
static_assert(std::is_same_v<decltype(Transform2D::position), atom::algo::Vec2>);
static_assert(std::is_same_v<decltype(Transform2D::rotation), float>);
static_assert(std::is_same_v<decltype(Transform2D::scale), atom::algo::Vec2>);
static_assert(std::is_same_v<decltype(SpriteRenderer::texture_uri), std::string>);
static_assert(std::is_same_v<decltype(SpriteRenderer::tint), atom::color::Color>);
static_assert(std::is_same_v<decltype(SpriteRenderer::layer), std::int32_t>);
static_assert(std::is_same_v<decltype(SpriteRenderer::sort_key), std::int32_t>);
static_assert(std::is_same_v<decltype(SpriteRenderer::flip_x), bool>);
static_assert(std::is_same_v<decltype(Camera2D::zoom), float>);
static_assert(std::is_same_v<decltype(Lifetime::remaining_seconds), float>);
static_assert(std::is_same_v<decltype(SceneEntityId::value), std::uint32_t>);

// --- ForEach bodies --------------------------------------------------------
//
// A function pointer plus a context, because the public header must not name the
// backend (see EntityWorld.hpp). Members are static so the pointer is a plain
// function, not a member pointer.

struct CopyTransformToDisplay {
        static auto Run(void* /*context*/, Entity /*entity*/, std::span<Transform2D> simulated,
                        std::span<RenderTransform2D> display) -> void {
            display[0].position = simulated[0].position;
            display[0].rotation = simulated[0].rotation;
            display[0].scale = simulated[0].scale;
        }
};

struct CollectCamera {
        static auto Run(void* context, Entity entity, std::span<Transform2D> transform,
                        std::span<Camera2D> camera) -> void {
            auto* found = static_cast<std::vector<std::pair<Entity, float>>*>(context);
            // Both spans must describe the same entity; adding the two numbers is
            // how the test proves the body sees the pairing, not just a count.
            found->emplace_back(entity, camera[0].zoom + transform[0].position.GetX());
        }
};

struct AgeLifetime {
        static auto Run(void* context, Entity entity, std::span<Lifetime> lifetimes) -> void {
            lifetimes[0].remaining_seconds -= 0.25f;
            if (lifetimes[0].remaining_seconds <= 0.0f)
                static_cast<std::vector<Entity>*>(context)->push_back(entity);
        }
};

} // namespace

auto main() -> int {
    auto created = EntityWorld::Create();
    ATOM_CHECK(created.has_value());
    auto& world = **created;

    // --- 2. Defaults are the documented ones ------------------------------
    {
        const Transform2D transform{};
        ATOM_CHECK(transform.rotation == 0.0f);
        ATOM_CHECK(transform.scale.GetX() == 1.0f && transform.scale.GetY() == 1.0f);

        const SpriteRenderer sprite{};
        ATOM_CHECK(sprite.texture_uri.empty());
        ATOM_CHECK(sprite.source_size.GetX() == 0.0f && sprite.source_size.GetY() == 0.0f);
        ATOM_CHECK(sprite.size.GetX() == 0.0f && sprite.size.GetY() == 0.0f);
        ATOM_CHECK(sprite.pivot.GetX() == 0.5f && sprite.pivot.GetY() == 0.5f);
        ATOM_CHECK(sprite.tint.r == 255 && sprite.tint.g == 255 && sprite.tint.b == 255 && sprite.tint.a == 255);
        ATOM_CHECK(sprite.layer == 0 && sprite.sort_key == 0);
        ATOM_CHECK(!sprite.flip_x && !sprite.flip_y);

        const Camera2D camera{};
        ATOM_CHECK(camera.zoom == 1.0f);
        ATOM_CHECK(camera.viewport_size.GetX() == 0.0f && camera.viewport_size.GetY() == 0.0f);

        const SceneEntityId id{};
        ATOM_CHECK(id.value == 0);
    }

    // --- 3. Emplace / read / patch / remove for every new type ------------
    {
        const Entity entity = world.CreateEntity();
        ATOM_CHECK(world.IsAlive(entity));

        ATOM_CHECK(world.Emplace<Transform2D>(entity, Transform2D{.position = {8.0f, 12.0f}}).has_value());
        ATOM_CHECK(world.Emplace<RenderTransform2D>(entity).has_value());
        ATOM_CHECK(world.Emplace<Camera2D>(entity, Camera2D{.zoom = 2.0f}).has_value());
        ATOM_CHECK(world.Emplace<Lifetime>(entity, Lifetime{.remaining_seconds = 1.5f}).has_value());
        ATOM_CHECK(world.Emplace<SceneEntityId>(entity, SceneEntityId{.value = 42}).has_value());

        SpriteRenderer sprite{};
        sprite.texture_uri = "res://textures/player.png";
        sprite.source_size = {16.0f, 16.0f};
        sprite.layer = 3;
        sprite.flip_x = true;
        ATOM_CHECK(world.Emplace<SpriteRenderer>(entity, sprite).has_value());

        // Every one of them is readable back through the public API.
        auto* transform = world.TryGet<Transform2D>(entity);
        ATOM_CHECK(transform != nullptr);
        ATOM_CHECK(transform->position.GetX() == 8.0f && transform->position.GetY() == 12.0f);

        auto* stored_sprite = world.TryGet<SpriteRenderer>(entity);
        ATOM_CHECK(stored_sprite != nullptr);
        ATOM_CHECK(stored_sprite->texture_uri == "res://textures/player.png");
        ATOM_CHECK(stored_sprite->layer == 3);
        ATOM_CHECK(stored_sprite->flip_x);

        auto* stored_display = world.TryGet<RenderTransform2D>(entity);
        ATOM_CHECK(stored_display != nullptr);
        ATOM_CHECK(stored_display->scale.GetX() == 1.0f);

        auto* stored_camera = world.TryGet<Camera2D>(entity);
        ATOM_CHECK(stored_camera != nullptr && stored_camera->zoom == 2.0f);

        auto* stored_id = world.TryGet<SceneEntityId>(entity);
        ATOM_CHECK(stored_id != nullptr && stored_id->value == 42);

        ATOM_CHECK(world.CountOf<Transform2D>() == 1);
        ATOM_CHECK(world.CountOf<SpriteRenderer>() == 1);
        ATOM_CHECK(world.CountOf<SceneEntityId>() == 1);

        // Patch keeps the type's identity and only replaces the value.
        ATOM_CHECK(world.Patch<Lifetime>(entity, Lifetime{.remaining_seconds = 0.5f}).has_value());
        ATOM_CHECK(world.TryGet<Lifetime>(entity)->remaining_seconds == 0.5f);

        // Emplace refuses a silent overwrite; that is what makes a scene load
        // able to report a duplicate component instead of losing one.
        ATOM_CHECK(!world.Emplace<Transform2D>(entity).has_value());

        ATOM_CHECK(world.Remove<SceneEntityId>(entity).has_value());
        ATOM_CHECK(!world.Has<SceneEntityId>(entity));

        ATOM_CHECK(world.Destroy(entity).has_value());
        ATOM_CHECK(!world.IsAlive(entity));
        ATOM_CHECK(world.CountOf<Transform2D>() == 0);
    }

    // --- 4. The new query sets return exactly the right entities ----------
    {
        // e_sprite: transform + display + sprite.
        // e_plain : transform only.
        // e_camera: transform + camera.
        // e_orphan: display + sprite, deliberately no transform.
        const Entity e_sprite = world.CreateEntity();
        const Entity e_plain = world.CreateEntity();
        const Entity e_camera = world.CreateEntity();
        const Entity e_orphan = world.CreateEntity();

        ATOM_CHECK(world.Emplace<Transform2D>(e_sprite).has_value());
        ATOM_CHECK(world.Emplace<RenderTransform2D>(e_sprite).has_value());
        ATOM_CHECK(world.Emplace<SpriteRenderer>(e_sprite).has_value());

        ATOM_CHECK(world.Emplace<Transform2D>(e_plain).has_value());

        ATOM_CHECK(world.Emplace<Transform2D>(e_camera).has_value());
        ATOM_CHECK(world.Emplace<Camera2D>(e_camera, Camera2D{.zoom = 2.0f}).has_value());

        ATOM_CHECK(world.Emplace<RenderTransform2D>(e_orphan).has_value());
        ATOM_CHECK(world.Emplace<SpriteRenderer>(e_orphan).has_value());

        const auto transforms = world.With<Transform2D>();
        ATOM_CHECK(transforms.size() == 3);

        const auto snapshots = world.With<Transform2D, RenderTransform2D>();
        ATOM_CHECK(snapshots.size() == 1);
        ATOM_CHECK(snapshots[0] == e_sprite);

        const auto drawable = world.With<RenderTransform2D, SpriteRenderer>();
        ATOM_CHECK(drawable.size() == 2);
        ATOM_CHECK(drawable[0] == e_sprite);
        ATOM_CHECK(drawable[1] == e_orphan);
        // Ascending entity index, not insertion or pool order.
        ATOM_CHECK(drawable[0].Index() < drawable[1].Index());

        const auto cameras = world.With<Transform2D, Camera2D>();
        ATOM_CHECK(cameras.size() == 1);
        ATOM_CHECK(cameras[0] == e_camera);
    }

    // --- 5. ForEach writes through the spans ------------------------------
    {
        // Give every simulated transform a distinct x so the copy is observable.
        for (const auto entity : world.With<Transform2D, RenderTransform2D>()) {
            auto* transform = world.TryGet<Transform2D>(entity);
            transform->position = {static_cast<float>(entity.Index()) + 1.0f, 5.0f};
            transform->scale = {2.0f, 3.0f};
        }

        ATOM_CHECK(world.ForEach<Transform2D, RenderTransform2D>({&CopyTransformToDisplay::Run, nullptr}).has_value());

        for (const auto entity : world.With<Transform2D, RenderTransform2D>()) {
            const auto* simulated = world.TryGet<Transform2D>(entity);
            const auto* display = world.TryGet<RenderTransform2D>(entity);
            ATOM_CHECK_NEAR(display->position.GetX(), simulated->position.GetX());
            ATOM_CHECK_NEAR(display->position.GetY(), simulated->position.GetY());
            ATOM_CHECK_NEAR(display->scale.GetY(), simulated->scale.GetY());
        }

        // The camera set: the body must see both components of the same entity,
        // which is what an active-camera lookup relies on.
        std::vector<std::pair<Entity, float>> visited{};
        ATOM_CHECK(world.ForEach<Transform2D, Camera2D>({&CollectCamera::Run, &visited}).has_value());
        ATOM_CHECK(visited.size() == 1);
        ATOM_CHECK(world.Has<Camera2D>(visited.front().first));
        ATOM_CHECK_NEAR(visited.front().second, 2.0f + 0.0f);
    }

    // --- 6. Lifetime: age, collect, then mark (never mid-iteration) -------
    {
        const Entity short_lived = world.CreateEntity();
        const Entity long_lived = world.CreateEntity();
        ATOM_CHECK(world.Emplace<Lifetime>(short_lived, Lifetime{.remaining_seconds = 0.25f}).has_value());
        ATOM_CHECK(world.Emplace<Lifetime>(long_lived, Lifetime{.remaining_seconds = 5.0f}).has_value());

        std::vector<Entity> expired{};
        ATOM_CHECK(world.ForEach<Lifetime>({&AgeLifetime::Run, &expired}).has_value());
        ATOM_CHECK(expired.size() == 1);
        ATOM_CHECK(expired.front() == short_lived);

        // Structural changes happen after the pass, exactly as the sweep requires.
        for (const auto entity : expired)
            ATOM_CHECK(world.Emplace<PendingDestroy>(entity).has_value());

        const auto doomed = world.With<PendingDestroy>();
        ATOM_CHECK(doomed.size() == 1);
        for (const auto entity : doomed)
            ATOM_CHECK(world.Destroy(entity).has_value());

        ATOM_CHECK(world.Has<Lifetime>(long_lived));
        ATOM_CHECK_NEAR(world.TryGet<Lifetime>(long_lived)->remaining_seconds, 4.75f);
    }

    return 0;
}

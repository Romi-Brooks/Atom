/**
 * @file           : WorldReplaceTest.cpp
 * @brief          : T2.2 - replacing a world's entities without losing its behaviour.
 *
 * A scene switch must do exactly one thing: change which entities exist. Everything
 * that makes a world a *running* world - the systems registered on it and the frame
 * counter - has to survive, otherwise loading a level would silently unregister the
 * systems the screen set up.
 *
 * The other half of the contract is the opposite direction: the buffers being
 * replaced must be dropped. A queued command names an entity of the old storage, so
 * applying it to the new one would address an unrelated entity (or a dead handle).
 */

#include <Runtime/GameWorld.hpp>

#include <ECS/Components.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <memory>
#include <vector>

namespace {

using namespace atom;
using runtime::FrameContext;
using runtime::GameWorld;
using runtime::SystemDescriptor;
using runtime::SystemPhase;

auto MakeWorldWithPlayer(ecs::Entity& player_out) -> std::unique_ptr<GameWorld> {
    auto created = GameWorld::Create();
    if (!created.has_value())
        return nullptr;
    auto world = std::move(*created);
    auto& entities = world->GetEntities();
    player_out = entities.CreateEntity();
    if (!entities.Emplace<ecs::Transform2D>(player_out, ecs::Transform2D{.position = {1.0F, 2.0F}}).has_value())
        return nullptr;
    if (!entities.Emplace<ecs::RenderTransform2D>(player_out).has_value())
        return nullptr;
    if (!entities.Emplace<ecs::SceneEntityId>(player_out, ecs::SceneEntityId{.value = 42}).has_value())
        return nullptr;
    return world;
}

} // namespace

auto main() -> int {
    // --- 1. Systems and the frame counter survive the swap ----------------
    {
        ecs::Entity old_player{};
        auto world = MakeWorldWithPlayer(old_player);
        ATOM_CHECK(world != nullptr);

        // A system that counts fixed steps: it must keep running after the swap.
        auto fixed_calls = 0;
        ATOM_CHECK(world->GetScheduler()
                       .Add(SystemPhase::FixedUpdate,
                            SystemDescriptor{"count", [&fixed_calls](GameWorld& /*w*/, const FrameContext& /*c*/) {
                                                 ++fixed_calls;
                                                 return Ok();
                                             }})
                       .has_value());

        ATOM_CHECK(world->RunFrame(0.016F, 1, 0.016F).has_value());
        ATOM_CHECK(fixed_calls == 1);
        ATOM_CHECK(world->GetFrameIndex() == 1);

        // The replacement storage: one different entity, no SceneEntityId 42.
        auto replacement = ecs::EntityWorld::Create();
        ATOM_CHECK(replacement.has_value());
        const auto new_entity = (*replacement)->CreateEntity();
        ATOM_CHECK((*replacement)->Emplace<ecs::Transform2D>(new_entity).has_value());

        auto previous = world->ReplaceEntities(std::move(*replacement));
        ATOM_CHECK(previous != nullptr); // the caller decides when the old world dies

        ATOM_CHECK(world->GetEntities().AliveCount() == 1);
        // The old entity's scene id is gone, which is the reliable way to state "that
        // entity no longer exists". IsAlive(old_player) is NOT usable here: the new
        // world's first entity gets the same (index, generation) pair, so a stale
        // handle can be "alive" again by accident - see GetWorldGeneration().
        ATOM_CHECK(world->GetEntities().CountOf<ecs::SceneEntityId>() == 0);
        ATOM_CHECK(world->GetWorldGeneration() == 1);

        // The system still runs, and the frame index continues rather than resetting:
        // a scene load is an event inside a session, not a new session.
        ATOM_CHECK(world->RunFrame(0.016F, 2, 0.016F).has_value());
        ATOM_CHECK(fixed_calls == 3);
        ATOM_CHECK(world->GetFrameIndex() == 2);
    }

    // --- 2. Pending commands and queued events are dropped ----------------
    {
        ecs::Entity old_player{};
        auto world = MakeWorldWithPlayer(old_player);
        ATOM_CHECK(world != nullptr);

        // Queue a destroy for an old-world entity, then swap before it is committed.
        // Flushing it afterwards would target a handle the new storage never issued.
        world->GetCommands().QueueDestroy(old_player);
        ATOM_CHECK(world->GetCommands().Count() == 1);

        world->GetEvents().Enqueue(runtime::GameEvent{1, old_player, old_player});
        ATOM_CHECK(world->GetEvents().Count() == 1);

        auto replacement = ecs::EntityWorld::Create();
        ATOM_CHECK(replacement.has_value());
        (void)(*replacement)->CreateEntity();

        auto previous = world->ReplaceEntities(std::move(*replacement));
        ATOM_CHECK(previous != nullptr);
        ATOM_CHECK(world->GetCommands().Count() == 0);
        ATOM_CHECK(world->GetEvents().Count() == 0);

        // A frame right after the swap must not try to apply the stale command.
        ATOM_CHECK(world->RunFrame(0.016F, 1, 0.016F).has_value());
        ATOM_CHECK(world->GetEntities().AliveCount() == 1);
    }

    // --- 3. A null replacement is refused, not applied ---------------------
    {
        ecs::Entity old_player{};
        auto world = MakeWorldWithPlayer(old_player);
        ATOM_CHECK(world != nullptr);

        ATOM_CHECK(world->ReplaceEntities(nullptr) == nullptr);
        // The world is untouched: a failed scene load must leave the running world
        // exactly as it was.
        ATOM_CHECK(world->GetEntities().AliveCount() == 1);
        ATOM_CHECK(world->GetEntities().IsAlive(old_player));
        ATOM_CHECK(world->GetEntities().CountOf<ecs::SceneEntityId>() == 1);
    }

    // --- 4. Systems can re-resolve entities by scene id -------------------
    //
    // This is the pattern a scene-aware system has to follow, because an Entity
    // handle cached before a swap is dead afterwards. Caching and re-resolving on
    // failure is what makes the same system correct across a level change.
    {
        ecs::Entity old_player{};
        auto world = MakeWorldWithPlayer(old_player);
        ATOM_CHECK(world != nullptr);

        // The cache is (generation, entity), not just the handle: the generation is
        // the only thing that tells a system "the world was swapped under you".
        struct Lookup final {
                std::uint64_t generation = 0;
                std::optional<ecs::Entity> cached{};
                std::size_t resolved = 0;
                std::size_t unresolved = 0;
        };
        auto lookup = std::make_shared<Lookup>();

        ATOM_CHECK(world->GetScheduler()
                       .Add(SystemPhase::Update,
                            SystemDescriptor{"find-player", [lookup](GameWorld& w, const FrameContext& /*c*/) {
                                                 auto& entities = w.GetEntities();
                                                 const auto generation = w.GetWorldGeneration();
                                                 if (lookup->generation == generation && lookup->cached.has_value() &&
                                                     entities.IsAlive(*lookup->cached)) {
                                                     ++lookup->resolved;
                                                     return Ok();
                                                 }
                                                 // Resolve by the stable scene id. Never trust a
                                                 // handle cached across a swap.
                                                 lookup->generation = generation;
                                                 lookup->cached.reset();
                                                 for (const auto entity : entities.With<ecs::SceneEntityId>()) {
                                                     const auto* id = entities.TryGet<ecs::SceneEntityId>(entity);
                                                     if (id != nullptr && id->value == 42) {
                                                         lookup->cached = entity;
                                                         ++lookup->resolved;
                                                         return Ok();
                                                     }
                                                 }
                                                 ++lookup->unresolved;
                                                 return Ok();
                                             }})
                       .has_value());

        ATOM_CHECK(world->RunFrame(0.016F, 1, 0.016F).has_value());
        ATOM_CHECK(lookup->cached.has_value());
        ATOM_CHECK(*lookup->cached == old_player);
        ATOM_CHECK(lookup->resolved == 1);
        ATOM_CHECK(lookup->unresolved == 0);

        // Replace with a world that has a *different* entity carrying the same scene
        // id. The cached handle is dead, so the system must re-resolve rather than
        // acting on a dead entity.
        auto replacement = ecs::EntityWorld::Create();
        ATOM_CHECK(replacement.has_value());
        const auto fresh = (*replacement)->CreateEntity();
        ATOM_CHECK((*replacement)->Emplace<ecs::SceneEntityId>(fresh, ecs::SceneEntityId{.value = 42}).has_value());
        auto previous = world->ReplaceEntities(std::move(*replacement));
        ATOM_CHECK(previous != nullptr);
        ATOM_CHECK(world->GetWorldGeneration() == 1);

        ATOM_CHECK(world->RunFrame(0.016F, 1, 0.016F).has_value());
        ATOM_CHECK(lookup->cached.has_value());
        ATOM_CHECK(*lookup->cached == fresh);
        ATOM_CHECK(lookup->resolved == 2);
        ATOM_CHECK(lookup->unresolved == 0);
    }

    return 0;
}

/**
 * @file           : EntityWorldTour.cpp
 * @brief          : A self-contained tour of the Atom ECS. Walks the API step by
 *                   step - entity lifecycle, component insert/read/replace, typed
 *                   iteration driving a real integration loop, tags, deferred
 *                   destroys and the cost of the loop - printing each result so the
 *                   reader can follow along without reading engine source.
 *
 * Run it from the command line. It needs no window, no GPU and no assets: the ECS
 * is pure storage, so the whole module can be exercised headless. That is also why
 * this example doubles as the template for the first physics system, which is the
 * next consumer of this module.
 *
 * Copyright (c) 2026 Romi Brooks, All rights reserved.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include <ECS/Components.hpp>
#include <ECS/EntityWorld.hpp>

namespace {

using atom::ecs::Active;
using atom::ecs::Entity;
using atom::ecs::EntityWorld;
using atom::ecs::PendingDestroy;
using atom::ecs::Position;
using atom::ecs::Velocity;

// A tiny section header so the console output reads like a guided tour.
auto Header(const std::string& title) -> void {
    std::cout << "\n==== " << title << " " << std::string(56 - std::min<std::size_t>(56, title.size()), '=') << "\n";
}

auto Print(const std::string& text) -> void {
    std::cout << "  " << text << "\n";
}

auto Describe(const Entity entity) -> std::string {
    if (!entity.IsValid())
        return "invalid";
    return "index=" + std::to_string(entity.Index()) + "/gen=" + std::to_string(entity.Generation());
}

// Everything the integration visitor needs that is not a component. A real system
// holds these as members; the tour packs them into one struct because the visitor
// signature is fixed by the engine.
struct IntegrateStats final {
        EntityWorld* world = nullptr;
        float delta_seconds = 0.0f;
        int visited = 0;
        int skipped_inactive = 0;
};

// The visitor for the integration loop. It is a plain function rather than a lambda
// because EntityWorld.hpp must not include the storage backend: the body has to be
// compiled in a translation unit that can see it, and a function pointer is what a
// backend-free header can carry. The parameters still arrive with their real types,
// so there is no void* casting at the call site.
//
// Components arrive as length-1 spans, which is why the body writes positions[0].
struct Integrate final {
        static auto Run(void* context, Entity entity, std::span<Position> positions, std::span<Velocity> velocities)
            -> void {
            auto& stats = *static_cast<IntegrateStats*>(context);

            // Tags cannot be listed in the query: the backend omits empty types
            // from its callback, so a visitor signature naming one could never be
            // satisfied. A tag is checked per entity instead, which is how a system
            // skips inactive entities.
            if (!stats.world->Has<Active>(entity)) {
                ++stats.skipped_inactive;
                return;
            }

            // position += velocity * dt
            positions[0].value = positions[0].value + velocities[0].value * stats.delta_seconds;
            ++stats.visited;
        }
};

} // namespace

auto main() -> int {
    // --- world creation ---------------------------------------------------
    Header("Creating the world");

    auto created = EntityWorld::Create();
    if (!created.has_value()) {
        std::cout << "  world creation failed: " << created.error().Message() << "\n";
        return 1;
    }
    auto world = std::move(*created);
    Print("world created; alive=" + std::to_string(world->AliveCount()));

    // --- entity lifecycle -------------------------------------------------
    Header("Entity lifecycle: create, recycle, destroy");

    const auto player = world->CreateEntity();
    const auto enemy = world->CreateEntity();
    Print("player  " + Describe(player) + "  alive=" + (world->IsAlive(player) ? "yes" : "no"));
    Print("enemy   " + Describe(enemy) + "  alive=" + (world->IsAlive(enemy) ? "yes" : "no"));

    // Destroying frees the slot. The engine reports failure rather than silently
    // succeeding, so a caller can tell "I destroyed it" from "something else did".
    const auto destroyed = world->Destroy(enemy);
    Print("destroy enemy: " + std::string{destroyed.has_value() ? "ok" : destroyed.error().Message()});
    Print("alive=" + std::to_string(world->AliveCount()));

    const auto destroyed_again = world->Destroy(enemy);
    Print("destroy enemy again: " + std::string{destroyed_again.has_value() ? "ok" : destroyed_again.error().Message()});

    // The freed slot is handed out again, at a new generation. That generation is
    // what makes the handle the caller still holds harmless.
    const auto respawned = world->CreateEntity();
    Print("respawned " + Describe(respawned));
    Print("same index as the destroyed one? " + std::string{respawned.Index() == enemy.Index() ? "yes" : "no"});
    Print("stale handle still alive? " + std::string{world->IsAlive(enemy) ? "YES (bug)" : "no"});

    // --- components -------------------------------------------------------
    Header("Components: insert, read, replace, remove");

    const auto inserted = world->Emplace<Position>(player, Position{{0.0f, 0.0f}});
    Print("emplace Position on player: " + std::string{inserted.has_value() ? "ok" : inserted.error().Message()});

    // Inserting twice would silently overwrite the stored value if the engine let
    // it through, so it is refused and the caller asks to replace explicitly.
    const auto duplicate = world->Emplace<Position>(player, Position{{99.0f, 99.0f}});
    Print("emplace Position again: " + std::string{duplicate.has_value() ? "ok" : duplicate.error().Message()});
    Print("value survived the rejected insert: x=" + std::to_string(world->TryGet<Position>(player)->value.GetX()));

    const auto replaced = world->EmplaceOrReplace<Position>(player, Position{{1.0f, 2.0f}});
    Print("emplace-or-replace: " + std::string{replaced.has_value() ? "ok" : replaced.error().Message()});
    Print("value now: x=" + std::to_string(world->TryGet<Position>(player)->value.GetX()));

    // Reading a component the entity does not own is an error, not a crash.
    const auto absent = world->Get<Velocity>(player);
    Print("Get Velocity (not owned): " + std::string{absent.has_value() ? "ok" : absent.error().Message()});

    const auto patched = world->Patch<Position>(player, Position{{5.0f, 6.0f}});
    Print("patch Position: " + std::string{patched.has_value() ? "ok" : patched.error().Message()});

    // --- a real integration loop -----------------------------------------
    Header("Typed iteration: a fixed-step integration pass");

    // A small scene built so the two ways an entity can be left out are not
    // confused with each other:
    //
    //   add(x, v, true)  Position + Velocity + Active  -> integrated
    //   add(x, v, false) Position + Velocity, inactive -> selected by the query,
    //                                                     skipped by the tag check
    //   add(x, 0, ...)   Position, no Velocity         -> not selected at all
    struct Entry final {
            Entity entity{};
            std::string note{};
    };
    std::vector<Entry> scene{};

    const auto add = [&](const float x, const float vx, const bool active) {
        const auto entity = world->CreateEntity();
        world->Emplace<Position>(entity, Position{{x, 0.0f}});
        if (vx > 0.0f)
            world->Emplace<Velocity>(entity, Velocity{{vx, 0.0f}});
        if (active)
            world->Emplace<Active>(entity);
        scene.push_back(Entry{entity, {}});
    };

    add(0.0f, 1.0f, true);  // integrated
    add(100.0f, 10.0f, false); // has velocity, inactive: the tag check stops it
    add(200.0f, 0.0f, true);  // active but no Velocity: not in the query
    add(300.0f, 0.0f, false); // neither

    Print("scene: " + std::to_string(scene.size()) + " entities with Position, " +
          std::to_string(world->CountOf<Velocity>()) + " with Velocity, " + std::to_string(world->CountOf<Active>()) +
          " active");
    Print("ForEach<Position, Velocity> selects 2 of them; the tag check then skips 1");

    IntegrateStats stats{};
    stats.world = world.get();
    stats.delta_seconds = 0.5f;

    const auto visited = world->ForEach<Position, Velocity>({&Integrate::Run, &stats});
    if (!visited.has_value()) {
        std::cout << "  ForEach failed: " << visited.error().Message() << "\n";
        return 1;
    }
    Print("visited=" + std::to_string(stats.visited) + "  skipped as inactive=" +
          std::to_string(stats.skipped_inactive));

    std::cout << "  " << std::left << std::setw(22) << "entity" << std::setw(12) << "position.x" << std::setw(12)
              << "velocity.x" << std::setw(9) << "active" << "why\n";
    for (const auto& entry : scene) {
        const auto* position = world->TryGet<Position>(entry.entity);
        const auto* velocity = world->TryGet<Velocity>(entry.entity);
        const bool active = world->Has<Active>(entry.entity);

        std::string why = "visited: x += v * 0.5";
        if (velocity == nullptr)
            why = "not selected: no Velocity";
        else if (!active)
            why = "selected but skipped: inactive";

        std::cout << "  " << std::left << std::setw(22) << Describe(entry.entity) << std::setw(12)
                  << (position != nullptr ? position->value.GetX() : 0.0f) << std::setw(12)
                  << (velocity != nullptr ? velocity->value.GetX() : 0.0f) << std::setw(9)
                  << (active ? "yes" : "no") << why << "\n";
    }
    Print("the inactive entity has a velocity but did not move: the tag check stopped it, not the query");

    // --- tags -------------------------------------------------------------
    Header("Tags: presence is the whole datum");

    const auto tagged = world->CreateEntity();
    Print("Has<Active> before: " + std::string{world->Has<Active>(tagged) ? "yes" : "no"});
    world->Emplace<Active>(tagged);
    Print("Has<Active> after emplace: " + std::string{world->Has<Active>(tagged) ? "yes" : "no"});
    Print("CountOf<Active>=" + std::to_string(world->CountOf<Active>()));
    world->Emplace<PendingDestroy>(tagged);
    Print("marked PendingDestroy: the deferred sweep, so a system never restructures the world it walks");

    // A tag carries no value, so asking for one is a compile error rather than a
    // pointer to nothing. Uncomment either line to see it:
    //
    //     world->TryGet<Active>(tagged);
    //     world->ForEach<Active, Position>({nullptr, nullptr});

    // --- deferred structural changes -------------------------------------
    Header("Sweeping deferred destroys");

    const auto before = world->AliveCount();
    // Collect first, then destroy: With() materialises the list, so the sweep does
    // not mutate the storage it is walking.
    const auto doomed = world->With<PendingDestroy>();
    for (const auto entity : doomed) {
        // The result is checked rather than discarded: an entity that is already
        // gone would otherwise disappear silently from the sweep.
        if (const auto swept = world->Destroy(entity); !swept.has_value())
            Print("sweep skipped " + Describe(entity) + ": " + swept.error().Message());
    }
    Print("alive before sweep=" + std::to_string(before) + " after=" + std::to_string(world->AliveCount()));

    // --- cost -------------------------------------------------------------
    Header("Cost: what a fixed-step system actually pays");

    constexpr int kPasses = 10000;
    const auto start = std::chrono::steady_clock::now();
    for (int pass = 0; pass < kPasses; ++pass)
        world->ForEach<Position, Velocity>({&Integrate::Run, &stats});
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);

    const auto positioned = world->CountOf<Position>();
    Print(std::to_string(kPasses) + " passes over " + std::to_string(positioned) + " positioned entities in " +
          std::to_string(elapsed.count()) + "us total");
    Print("~" + std::to_string(elapsed.count() / kPasses) + "us per pass");
    Print("the loop allocates nothing and resolves no handle per entity");

    Header("Done");
    Print("See ECS/README.md for the boundary rule and how to add a component.");
    return 0;
}

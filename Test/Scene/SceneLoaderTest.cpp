/**
 * @file           : SceneLoaderTest.cpp
 * @brief          : T2.2 - scene validation, world building, and the failure paths.
 *
 * Two layers, tested separately on purpose:
 *
 *   ParseScene   pure text -> SceneDesc, with every rule from the v1 schema document
 *   BuildWorld   SceneDesc -> EntityWorld, with the component mapping
 *
 * Two sets of on-disk files are read here on purpose:
 *  - the demo room under Example/TestGame/Assets, because it ships with the engine and
 *    must not rot into an unloadable state;
 *  - the loader's own invalid fixtures under Test/Data/scenes, which exist only to
 *    prove that each rule rejects - a fixture that silently starts passing is a
 *    test that stopped working.
 */

#include <Scene/SceneLoader.hpp>
#include <Scene/SceneSchema.hpp>

#include <ECS/Components.hpp>
#include <Filesystem/FileSystem.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace atom;
using atom::scene::kNoActiveCamera;
using atom::scene::ParseOutcome;
using atom::scene::ParseScene;

auto HasDiagnostic(const ParseOutcome& outcome, const std::string_view needle) -> bool {
    for (const auto& diagnostic : outcome.diagnostics) {
        if (diagnostic.Format().find(needle) != std::string::npos)
            return true;
    }
    return false;
}

// A minimal valid document, with placeholders so each failure case changes exactly
// one thing and the diff in this file reads like the rule it tests.
auto MinimalScene(const std::string_view body) -> std::string {
    return "{\n  \"schema_version\": 1,\n  \"id\": \"test\",\n  \"entities\": [" + std::string{body} + "]\n}";
}

auto Fails(const std::string_view body, const std::string_view expected) -> bool {
    const auto outcome = ParseScene(MinimalScene(body), "res://scenes/test.scene.json");
    if (outcome.Ok())
        return false;
    return HasDiagnostic(outcome, expected);
}

} // namespace

auto main() -> int {
    // --- 1. The committed sample is valid and covers every v1 field --------
    {
        fs::AssetPath path{};
        ATOM_CHECK(fs::AssetPath::TryParse("res://scenes/test_room.scene.json", path));

        std::unique_ptr<fs::NativeFileSystem> backend{};
        ATOM_CHECK(fs::NativeFileSystem::Create("res", std::string{ATOM_SOURCE_ROOT} + "/Example/TestGame/Assets",
                                                backend) == fs::Result::Success);

        const auto loaded = scene::SceneLoader::Load(*backend, path);
        ATOM_CHECK_MSG(loaded.has_value(), loaded.has_value() ? "" : loaded.error().Message().c_str());

        const auto& description = loaded->description;
        // The shipped room is *example content*, so this test asserts invariants, not counts.
        // Pinning "10 entities / 9 sprites" here made every scene edit an engine-test edit, which
        // is the test-level version of a consumer bending the engine (RI-011).
        ATOM_CHECK(description.schema_version >= 1);
        ATOM_CHECK(!description.entities.empty());
        ATOM_CHECK(description.id == "test_room");
        ATOM_CHECK(description.has_name);
        ATOM_CHECK(description.active_camera != kNoActiveCamera);
        ATOM_CHECK(description.entities[description.active_camera].id == 1);

        ATOM_CHECK(loaded->entities != nullptr);
        auto& world = *loaded->entities;

        // Structural invariants every scene must satisfy, whatever the content is.
        ATOM_CHECK(world.CountOf<ecs::SceneEntityId>() == description.entities.size());
        ATOM_CHECK(world.CountOf<ecs::Transform2D>() == description.entities.size());
        ATOM_CHECK(world.CountOf<ecs::RenderTransform2D>() == description.entities.size());
        ATOM_CHECK(world.CountOf<ecs::Camera2D>() == 1);
        ATOM_CHECK(world.CountOf<ecs::Active>() >= 1);
        // The room is top-down: gravity comes from the document, not from the default.
        ATOM_CHECK_NEAR(description.gravity.GetY(), 0.0F);
        // A physics document without bodies would mean the v2 block never reached the world.
        ATOM_CHECK(world.CountOf<ecs::PhysicsBody2D>() >= 1);
        // The active camera got the lowest entity index, which is what Scene2D's
        // "lowest index wins" rule needs.
        const auto cameras = world.With<ecs::Transform2D, ecs::Camera2D>();
        ATOM_CHECK(cameras.size() == 1);
        const auto all_entities = world.With<ecs::SceneEntityId>();
        ATOM_CHECK(all_entities.front() == cameras.front());
        const auto* lens = world.TryGet<ecs::Camera2D>(cameras.front());
        ATOM_CHECK(lens != nullptr);
        ATOM_CHECK_NEAR(lens->zoom, 1.0F);

        // A sprite's fields survive the round trip: texture URI, source rect, size,
        // pivot, tint, layer, sort key and flip.
        std::size_t atlas_cells = 0;
        std::size_t flipped = 0;
        for (const auto entity : world.With<ecs::RenderTransform2D, ecs::SpriteRenderer>()) {
            const auto* sprite = world.TryGet<ecs::SpriteRenderer>(entity);
            ATOM_CHECK(sprite != nullptr);
            ATOM_CHECK(sprite->texture_uri.starts_with("res://textures/"));
            if (sprite->source_size.GetX() > 0.0F && sprite->source_size.GetY() > 0.0F)
                ++atlas_cells;
            if (sprite->flip_x)
                ++flipped;
        }
        ATOM_CHECK(atlas_cells == 1);
        ATOM_CHECK(flipped == 1);

        // The 3rd entity is a static crate with the authored sort key.
        // Tags are presence-only, so they are checked per entity rather than queried:
        // that keeps the ECS instantiation list to the sets with a real caller.
        std::size_t crates = 0;
        for (const auto entity : world.With<ecs::RenderTransform2D, ecs::SpriteRenderer>()) {
            if (!world.Has<ecs::Static>(entity))
                continue;
            const auto* sprite = world.TryGet<ecs::SpriteRenderer>(entity);
            ATOM_CHECK(sprite != nullptr);
            ATOM_CHECK(sprite->texture_uri == "res://textures/crate.png");
            ++crates;
        }
        ATOM_CHECK(crates == 2);

        // Lifetime came through with its authored duration.
        for (const auto entity : world.With<ecs::Lifetime>()) {
            const auto* lifetime = world.TryGet<ecs::Lifetime>(entity);
            ATOM_CHECK(lifetime != nullptr);
            ATOM_CHECK_NEAR(lifetime->remaining_seconds, 4.0F);
        }

        // Entities 3 and 4 are instances of res://prefabs/crate.prefab.json: the body
        // (texture, size, pivot, static tag) comes from that file, and only the
        // overrides (name, position, sort key) differ between them. This is the only
        // place the *shipped* prefab document is exercised, so a broken prefab file
        // fails here instead of at the demo's boot.
        for (const auto entity : world.With<ecs::RenderTransform2D, ecs::SpriteRenderer>()) {
            const auto* id = world.TryGet<ecs::SceneEntityId>(entity);
            if (id == nullptr || (id->value != 3 && id->value != 4))
                continue;
            const auto* sprite = world.TryGet<ecs::SpriteRenderer>(entity);
            const auto* name = world.TryGet<ecs::Name>(entity);
            const auto* transform = world.TryGet<ecs::Transform2D>(entity);
            ATOM_CHECK(sprite->texture_uri == "res://textures/crate.png");
            ATOM_CHECK(sprite->size.GetX() == 48.0F && sprite->size.GetY() == 48.0F);
            ATOM_CHECK(sprite->pivot.GetX() == 0.5F);
            ATOM_CHECK(world.Has<ecs::Static>(entity));
            ATOM_CHECK(name != nullptr);
            ATOM_CHECK(transform != nullptr);
            const bool left = id->value == 3;
            ATOM_CHECK(name->value == (left ? "crate_left" : "crate_right"));
            ATOM_CHECK(sprite->sort_key == (left ? 10 : 20));
                    // Positions from the room document; two crates at different sort keys is what makes the
        // draw order observable instead of accidental.
        ATOM_CHECK_NEAR(transform->position.GetX(), left ? 480.0F : 640.0F);
        }
    }

    // --- 2. The committed invalid fixtures are rejected, per rule ---------
    {
        std::unique_ptr<fs::NativeFileSystem> backend{};
        ATOM_CHECK(fs::NativeFileSystem::Create("res", std::string{ATOM_SOURCE_ROOT} + "/Test/Data/scenes", backend) ==
                   fs::Result::Success);

        struct Case {
                const char* uri;
                const char* expected;
        };
        const Case cases[] = {
            {"res://invalid_unknown_field.scene.json", "did you mean \"sprite\"?"},
            {"res://invalid_duplicate_id.scene.json", "duplicate entity id 7"},
            {"res://invalid_bad_version.scene.json", "unsupported schema_version 99"},
        };
        for (const auto& test_case : cases) {
            fs::AssetPath path{};
            ATOM_CHECK(fs::AssetPath::TryParse(test_case.uri, path));
            const auto loaded = scene::SceneLoader::Load(*backend, path);
            ATOM_CHECK_MSG(!loaded.has_value(), test_case.uri);
            ATOM_CHECK(loaded.error().Code() == ErrorCode::SceneInvalidField);
            // The message must locate the problem: uri, line:column, then the reason.
            const auto message = loaded.error().Detail();
            ATOM_CHECK_MSG(message.find(test_case.expected) != std::string::npos, message.data());
            ATOM_CHECK(message.find("res://invalid_") != std::string_view::npos);
            ATOM_CHECK(message.find(".json:") != std::string_view::npos);
        }

        // A missing file is a load failure, not a schema failure.
        fs::AssetPath missing{};
        ATOM_CHECK(fs::AssetPath::TryParse("res://nope.scene.json", missing));
        const auto absent = scene::SceneLoader::Load(*backend, missing);
        ATOM_CHECK(!absent.has_value());
        ATOM_CHECK(absent.error().Code() == ErrorCode::SceneLoadFailed);
    }

    // --- 3. Field rules from the schema document --------------------------
    {
        // Unknown fields are errors and carry a suggestion when the slip is close.
        ATOM_CHECK(Fails(R"({ "id": 1, "spirte": {} })", "did you mean \"sprite\"?"));
        ATOM_CHECK(Fails(R"({ "id": 1, "transform": { "postion": [0, 0] } })", "did you mean \"position\"?"));
        ATOM_CHECK(Fails(R"({ "id": 1, "sprite": { "texture": "res://a.png", "flip": true } })", "unknown field"));

        // Ids are identity: 0 is reserved, duplicates are errors, a negative is
        // rejected, and a fractional id is not an integer.
        ATOM_CHECK(Fails(R"({ "id": 0 })", "at least 1"));
        ATOM_CHECK(Fails(R"({ "id": -3 })", "unsigned 32-bit"));
        ATOM_CHECK(Fails(R"({ "id": 1.5 })", "fractional"));
        ATOM_CHECK(Fails(R"({ "id": 1 }, { "id": 1 })", "duplicate entity id 1"));
        ATOM_CHECK(Fails(R"({ "id": 4294967296 })", "unsigned 32-bit"));

        // Textures must be res:// URIs: a bare path or a traversal cannot be mounted.
        ATOM_CHECK(Fails(R"({ "id": 1, "sprite": { "texture": "textures/a.png" } })", "res://"));
        ATOM_CHECK(Fails(R"({ "id": 1, "sprite": { "texture": "res://../a.png" } })", "res://"));
        ATOM_CHECK(Fails(R"({ "id": 1, "sprite": {} })", "requires a \"texture\""));

        // Geometry that would produce a degenerate quad is rejected at the door.
        ATOM_CHECK(Fails(R"({ "id": 1, "transform": { "scale": [0, 1] } })", "non-zero"));
        ATOM_CHECK(Fails(R"({ "id": 1, "sprite": { "texture": "res://a.png", "size": [0, 8] } })", "greater than zero"));
        ATOM_CHECK(Fails(R"({ "id": 1, "sprite": { "texture": "res://a.png", "source": [0, 0, 0, 8] } })",
                         "greater than zero"));
        ATOM_CHECK(Fails(R"({ "id": 1, "camera": { "zoom": 0 } })", "greater than zero"));

        // Colours are 0..255 integers; "300" is not clamped silently.
        ATOM_CHECK(Fails(R"({ "id": 1, "sprite": { "texture": "res://a.png", "tint": [300, 0, 0, 255] } })", "0..255"));

        // Runtime-internal tags cannot be authored.
        ATOM_CHECK(Fails(R"({ "id": 1, "tags": ["pending_destroy"] })", "internal tag"));
        ATOM_CHECK(Fails(R"({ "id": 1, "tags": ["walking"] })", "unknown tag"));

        // Lifetime cannot go backwards.
        ATOM_CHECK(Fails(R"({ "id": 1, "lifetime": -1.0 })", "negative"));
    }

    // --- 4. Camera cardinality --------------------------------------------
    {
        // Two cameras, none marked active: refuse to guess.
        const auto ambiguous = ParseScene(MinimalScene(
                                             R"({ "id": 1, "camera": {} }, { "id": 2, "camera": {} })"),
                                         "res://scenes/test.scene.json");
        ATOM_CHECK(!ambiguous.Ok());
        ATOM_CHECK(HasDiagnostic(ambiguous, "none sets \"active\": true"));

        // Two cameras, both active: also ambiguous.
        const auto conflicting = ParseScene(MinimalScene(R"(
            { "id": 1, "camera": { "active": true } },
            { "id": 2, "camera": { "active": true } })"),
                                            "res://scenes/test.scene.json");
        ATOM_CHECK(!conflicting.Ok());
        ATOM_CHECK(HasDiagnostic(conflicting, "only one camera may set"));

        // Exactly one camera, unmarked: unambiguous, so it is used.
        const auto single = ParseScene(MinimalScene(R"({ "id": 1, "camera": {} })"),
                                       "res://scenes/test.scene.json");
        ATOM_CHECK(single.Ok());
        ATOM_CHECK(single.scene->active_camera == 0);

        // Two cameras, one marked: the marked one wins even though it is second in
        // the file; the loader then creates it first so Scene2D finds it.
        const auto marked = ParseScene(MinimalScene(R"(
            { "id": 1, "camera": { "active": false } },
            { "id": 2, "camera": { "active": true } })"),
                                       "res://scenes/test.scene.json");
        ATOM_CHECK(marked.Ok());
        ATOM_CHECK(marked.scene->active_camera == 1);
        const auto built = scene::SceneLoader::BuildWorld(*marked.scene);
        ATOM_CHECK(built.has_value());
        const auto cameras = (*built)->With<ecs::Transform2D, ecs::Camera2D>();
        ATOM_CHECK(cameras.size() == 2);
        // The active (id 2) camera is created first, so it holds the lowest index.
        ATOM_CHECK((*built)->TryGet<ecs::SceneEntityId>(cameras.front())->value == 2);
    }

    // --- 5. Diagnostics collect, they do not stop at the first error ------
    {
        const auto many = ParseScene(MinimalScene(R"(
            { "id": 1, "sprite": { "texture": "bad.png", "size": [0, 0], "tint": [300, 0, 0, 0] } },
            { "id": 2, "transform": { "scale": [0, 0] } },
            { "id": 3, "lifetime": -1.0 })"),
                                     "res://scenes/test.scene.json");
        ATOM_CHECK(!many.Ok());
        // One edit session should be enough, so the count matters.
        ATOM_CHECK(many.diagnostics.size() >= 4);
        ATOM_CHECK(HasDiagnostic(many, "entity 1"));
        ATOM_CHECK(HasDiagnostic(many, "entity 2"));
        ATOM_CHECK(HasDiagnostic(many, "entity 3"));
        // The document is rejected as a whole: no partial SceneDesc leaks out.
        ATOM_CHECK(!many.scene.has_value());
    }

    // --- 6. A syntax error is one diagnostic with a position --------------
    {
        const auto broken = ParseScene("{ \"schema_version\": 1, ", "res://scenes/test.scene.json");
        ATOM_CHECK(!broken.Ok());
        ATOM_CHECK(broken.diagnostics.size() == 1);
        ATOM_CHECK(broken.diagnostics.front().line > 0);
        ATOM_CHECK(broken.diagnostics.front().Format().find("res://scenes/test.scene.json:1:") == 0);
    }

    // --- 7. A document with no entities is legal --------------------------
    {
        const auto empty = ParseScene(R"({ "schema_version": 1, "id": "empty", "entities": [] })",
                                      "res://scenes/test.scene.json");
        ATOM_CHECK(empty.Ok());
        ATOM_CHECK(empty.scene->entities.empty());
        ATOM_CHECK(empty.scene->active_camera == kNoActiveCamera);
        const auto built = scene::SceneLoader::BuildWorld(*empty.scene);
        ATOM_CHECK(built.has_value());
        ATOM_CHECK((*built)->AliveCount() == 0);
    }

    // --- 8. v2 physics: pixels in the document, metres in the component ---
    //
    // The scene file stays in one unit system (pixels, like every other size), and the loader
    // divides by the scene's own pixels-per-metre. A test that only checked "a component
    // exists" would miss the exact bug this catches: a 100 px crate becoming a 100 m crate.
    {
        const auto outcome = ParseScene(R"({
            "schema_version": 2,
            "id": "physics_scene",
            "physics": { "pixels_per_meter": 50.0, "gravity": [0.0, -4.0] },
            "entities": [
                { "id": 1, "transform": { "position": [100.0, 200.0] },
                  "physics": { "kind": "dynamic", "shape": { "box": [50.0, 25.0] }, "density": 2.0 } },
                { "id": 2, "transform": { "position": [0.0, 400.0] },
                  "physics": { "kind": "static", "shape": { "circle": 25.0 }, "sensor": true } }
            ]
        })",
                                         "res://scenes/physics.scene.json");
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        ATOM_CHECK(outcome.scene->schema_version == 2);
        ATOM_CHECK_NEAR(outcome.scene->pixels_per_meter, 50.0F);
        ATOM_CHECK_NEAR(outcome.scene->gravity.GetY(), -4.0F);

        const auto built = scene::SceneLoader::BuildWorld(*outcome.scene);
        ATOM_CHECK(built.has_value());
        auto& world = **built;
        ATOM_CHECK(world.CountOf<ecs::PhysicsBody2D>() == 2);

        for (const auto entity : world.With<ecs::Transform2D, ecs::PhysicsBody2D>()) {
            const auto* shell = world.TryGet<ecs::PhysicsBody2D>(entity);
            const auto* id = world.TryGet<ecs::SceneEntityId>(entity);
            ATOM_CHECK(shell != nullptr);
            ATOM_CHECK(id != nullptr);
            if (id->value == 1) {
                ATOM_CHECK(shell->kind == ecs::PhysicsBodyKind::Dynamic);
                // 50 px half-extent at 50 px/m == 1 m; the y halved one is 0.5 m.
                ATOM_CHECK_NEAR(shell->half_extent.GetX(), 1.0F);
                ATOM_CHECK_NEAR(shell->half_extent.GetY(), 0.5F);
                ATOM_CHECK_NEAR(shell->density, 2.0F);
                ATOM_CHECK(!shell->is_sensor);
            } else {
                ATOM_CHECK(shell->kind == ecs::PhysicsBodyKind::Static);
                // 25 px radius at 50 px/m == 0.5 m.
                ATOM_CHECK_NEAR(shell->radius, 0.5F);
                ATOM_CHECK(shell->is_sensor);
            }
        }
    }

    // --- 9. v1 documents are still accepted (additive version bump) -------
    {
        const auto outcome = ParseScene(R"({
            "schema_version": 1,
            "id": "v1_scene",
            "entities": [ { "id": 1 } ]
        })",
                                         "res://scenes/v1.scene.json");
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        ATOM_CHECK(outcome.scene->schema_version == 1);
        // A v1 document has no physics block: the defaults must still be usable, otherwise
        // every old file would need editing just to keep loading.
        ATOM_CHECK_NEAR(outcome.scene->pixels_per_meter, 100.0F);
        ATOM_CHECK(outcome.scene->entities.front().physics.has_value() == false);
    }

    // --- 10. Malformed physics is rejected, not defaulted -----------------
    {
        auto bad = [](const std::string_view physics_block, const std::string_view expected) -> bool {
            const auto outcome = ParseScene(
                "{\n  \"schema_version\": 2,\n  \"id\": \"t\",\n  \"entities\": [\n"
                "    { \"id\": 1, \"physics\": " +
                    std::string{physics_block} + " }\n  ]\n}",
                "res://scenes/t.scene.json");
            if (outcome.Ok())
                return false;
            return HasDiagnostic(outcome, expected);
        };
        ATOM_CHECK(bad(R"({ "kind": "dynamic" })", "requires a \"shape\""));
        ATOM_CHECK(bad(R"({ "kind": "dynamic", "shape": {} })", "either \"box\" or \"circle\""));
        ATOM_CHECK(bad(R"({ "kind": "dynamic", "shape": { "box": [0, 10] } })", "greater than zero"));
        ATOM_CHECK(bad(R"({ "kind": "dynamic", "shape": { "box": [10, 10], "circle": 5 } })", "not both"));
        ATOM_CHECK(bad(R"({ "kind": "floaty", "shape": { "circle": 5 } })", "allowed: \"static\", \"kinematic\", \"dynamic\""));
    }

    return 0;
}

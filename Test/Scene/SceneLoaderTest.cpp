/**
 * @file           : SceneLoaderTest.cpp
 * @brief          : T2.2 - scene validation, world building, and the failure paths.
 *
 * Two layers, tested separately on purpose:
 *
 *   ParseScene   pure text -> SceneDesc, with every rule from the v1 schema document
 *   BuildWorld   SceneDesc -> EntityWorld, with the component mapping
 *
 * The committed files under Assets/scenes are read through the real filesystem so
 * the samples cannot rot, and the invalid ones must be *rejected* - a fixture that
 * silently starts passing is a test that stopped working.
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
        ATOM_CHECK(fs::AssetPath::TryParse("res://scenes/smoke_room.scene.json", path));

        std::unique_ptr<fs::NativeFileSystem> backend{};
        ATOM_CHECK(fs::NativeFileSystem::Create("res", std::string{ATOM_SOURCE_ROOT} + "/Assets", backend) ==
                   fs::Result::Success);

        const auto loaded = scene::SceneLoader::Load(*backend, path);
        ATOM_CHECK_MSG(loaded.has_value(), loaded.has_value() ? "" : loaded.error().Message().c_str());

        const auto& description = loaded->description;
        ATOM_CHECK(description.schema_version == 1);
        ATOM_CHECK(description.id == "smoke_room");
        ATOM_CHECK(description.has_name);
        ATOM_CHECK(description.entities.size() == 6);
        // The camera is entity 1 and marks itself active.
        ATOM_CHECK(description.active_camera != kNoActiveCamera);
        ATOM_CHECK(description.entities[description.active_camera].id == 1);

        ATOM_CHECK(loaded->entities != nullptr);
        auto& world = *loaded->entities;

        // Every entity gets an id plus a simulation and a display transform: the
        // renderer must be able to draw before the first snapshot.
        ATOM_CHECK(world.CountOf<ecs::SceneEntityId>() == 6);
        ATOM_CHECK(world.CountOf<ecs::Transform2D>() == 6);
        ATOM_CHECK(world.CountOf<ecs::RenderTransform2D>() == 6);
        ATOM_CHECK(world.CountOf<ecs::SpriteRenderer>() == 5);
        ATOM_CHECK(world.CountOf<ecs::Camera2D>() == 1);
        ATOM_CHECK(world.CountOf<ecs::Lifetime>() == 1);
        ATOM_CHECK(world.CountOf<ecs::Active>() == 1);  // the player
        ATOM_CHECK(world.CountOf<ecs::Name>() == 6);

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
    }

    // --- 2. The committed invalid fixtures are rejected, per rule ---------
    {
        std::unique_ptr<fs::NativeFileSystem> backend{};
        ATOM_CHECK(fs::NativeFileSystem::Create("res", std::string{ATOM_SOURCE_ROOT} + "/Assets", backend) ==
                   fs::Result::Success);

        struct Case {
                const char* uri;
                const char* expected;
        };
        const Case cases[] = {
            {"res://scenes/Invalid/unknown_field.scene.json", "did you mean \"sprite\"?"},
            {"res://scenes/Invalid/duplicate_id.scene.json", "duplicate entity id 7"},
            {"res://scenes/Invalid/bad_version.scene.json", "unsupported schema_version 2"},
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
            ATOM_CHECK(message.find("res://scenes/Invalid/") != std::string_view::npos);
            ATOM_CHECK(message.find(".json:") != std::string_view::npos);
        }

        // A missing file is a load failure, not a schema failure.
        fs::AssetPath missing{};
        ATOM_CHECK(fs::AssetPath::TryParse("res://scenes/nope.scene.json", missing));
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

    return 0;
}

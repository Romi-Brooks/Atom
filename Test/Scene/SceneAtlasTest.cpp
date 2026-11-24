/**
 * @file           : SceneAtlasTest.cpp
 * @brief          : Scene sprites that name a frame instead of a texel rectangle.
 *
 * The whole value of the atlas layer is that a scene entity says "the green cell" and
 * the numbers live in one art file. That only holds if the format keeps the two forms
 * exclusive: allowing `source` next to `atlas`/`frame` would put the rectangle back in
 * every entity, which is exactly what the atlas was added to stop.
 *
 * The atlas provider is supplied from memory here, so the schema's resolution rules are
 * tested without a filesystem; the shipped atlas is covered by Atom_Sprite.Atlas and
 * Atom_Scene.SceneLoader.
 */

#include <Scene/SceneSchema.hpp>
#include <Sprite/SpriteAtlas.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace atom;

constexpr std::string_view kAtlas = R"({
  "schema_version": 1,
  "kind": "atlas",
  "id": "terrain",
  "texture": "res://textures/atlas.png",
  "filter": "nearest",
  "frames": {
    "green": { "rect": [8, 0, 8, 8], "size": [64.0, 64.0] },
    "yellow": { "rect": [8, 8, 8, 8], "size": [64.0, 64.0], "pivot": [0.5, 1.0] }
  }
})";

auto Scene(std::string_view body) -> std::string {
    return "{\n  \"schema_version\": 1,\n  \"id\": \"test\",\n  \"entities\": [" + std::string{body} + "]\n}";
}

auto HasDiagnostic(const scene::ParseOutcome& outcome, const std::string_view needle) -> bool {
    for (const auto& diagnostic : outcome.diagnostics) {
        if (diagnostic.Format().find(needle) != std::string::npos)
            return true;
    }
    return false;
}

} // namespace

auto main() -> int {
    // One parsed atlas, borrowed by the provider for the whole test.
    const auto parsed = sprite::ParseAtlas(kAtlas, "res://atlases/terrain.atlas.json");
    ATOM_CHECK_MSG(parsed.Ok(), parsed.Ok() ? "" : parsed.diagnostics.front().Format().c_str());
    const auto& atlas = *parsed.atlas;

    std::size_t provider_calls = 0;
    const scene::AtlasProvider provider =
        [&atlas, &provider_calls](const std::string_view uri) -> Result<const sprite::SpriteAtlas*> {
        ++provider_calls;
        if (uri != "res://atlases/terrain.atlas.json") {
            return std::unexpected<Error>{
                MakeError(ErrorCode::InvalidDocument, Module::Core, "test provider")
                    .WithDetail("no atlas at " + std::string{uri})};
        }
        return &atlas;
    };

    // --- 1. A frame fills texture, rect, size, pivot and filter ------------
    {
        const auto outcome = scene::ParseScene(Scene(R"({
            "id": 1,
            "sprite": { "atlas": "res://atlases/terrain.atlas.json", "frame": "green", "layer": 3 }
        })"),
                                               "res://scenes/test.scene.json", {}, provider);
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        const auto& sprite = *outcome.scene->entities.front().sprite;

        // The texture comes from the atlas, not from the entity.
        ATOM_CHECK(sprite.texture_uri == "res://textures/atlas.png");
        ATOM_CHECK(sprite.has_source);
        ATOM_CHECK(sprite.source.x == 8.0F && sprite.source.width == 8.0F);
        ATOM_CHECK(sprite.has_size);
        ATOM_CHECK(sprite.size.GetX() == 64.0F);
        ATOM_CHECK(sprite.pivot.GetX() == 0.5F && sprite.pivot.GetY() == 0.5F);
        // Pixel art stays crisp because the sheet said so, and the entity cannot forget.
        ATOM_CHECK(sprite.filter == ecs::SpriteFilter::Nearest);
        // Per-instance fields still come from the entity.
        ATOM_CHECK(sprite.layer == 3);
        ATOM_CHECK(provider_calls == 1);
    }

    // --- 2. The pivot is the frame's, not the default ---------------------
    {
        const auto outcome = scene::ParseScene(Scene(R"({
            "id": 1,
            "sprite": { "atlas": "res://atlases/terrain.atlas.json", "frame": "yellow" }
        })"),
                                               "res://scenes/test.scene.json", {}, provider);
        ATOM_CHECK(outcome.Ok());
        ATOM_CHECK(outcome.scene->entities.front().sprite->pivot.GetY() == 1.0F);
    }

    // --- 3. The two forms cannot be mixed ---------------------------------
    //
    // Each of these would put a second source of truth next to the atlas.
    {
        for (const char* field : {"source", "size", "pivot", "texture"}) {
            const auto outcome = scene::ParseScene(
                Scene(std::string{R"({ "id": 1, "sprite": { "atlas": "res://atlases/terrain.atlas.json",
                                                            "frame": "green", ")"} +
                      field + R"(": [0, 0, 8, 8] } })"),
                "res://scenes/test.scene.json", {}, provider);
            ATOM_CHECK_MSG(!outcome.Ok(), field);
            ATOM_CHECK_MSG(HasDiagnostic(outcome, "is defined by the atlas frame"), field);
        }
    }

    // --- 4. Frame and atlas are required together -------------------------
    {
        auto missing_frame = scene::ParseScene(
            Scene(R"({ "id": 1, "sprite": { "atlas": "res://atlases/terrain.atlas.json" } })"),
            "res://scenes/test.scene.json", {}, provider);
        ATOM_CHECK(!missing_frame.Ok());
        ATOM_CHECK(HasDiagnostic(missing_frame, "requires a \"frame\""));

        auto missing_atlas = scene::ParseScene(Scene(R"({ "id": 1, "sprite": { "frame": "green" } })"),
                                              "res://scenes/test.scene.json", {}, provider);
        ATOM_CHECK(!missing_atlas.Ok());
        ATOM_CHECK(HasDiagnostic(missing_atlas, "requires an \"atlas\""));
    }

    // --- 5. A missing frame lists what the atlas does have ----------------
    //
    // "no frame \"gren\" (available: green, yellow)" is a one-glance fix; a generic
    // resolution failure would send the author back to the atlas file.
    {
        const auto outcome = scene::ParseScene(
            Scene(R"({ "id": 1, "sprite": { "atlas": "res://atlases/terrain.atlas.json", "frame": "gren" } })"),
            "res://scenes/test.scene.json", {}, provider);
        ATOM_CHECK(!outcome.Ok());
        ATOM_CHECK(HasDiagnostic(outcome, "has no frame \"gren\""));
        ATOM_CHECK(HasDiagnostic(outcome, "available: green, yellow"));
    }

    // --- 6. A missing atlas is a scene diagnostic, not a crash ------------
    {
        const auto outcome = scene::ParseScene(
            Scene(R"({ "id": 1, "sprite": { "atlas": "res://atlases/nope.atlas.json", "frame": "green" } })"),
            "res://scenes/test.scene.json", {}, provider);
        ATOM_CHECK(!outcome.Ok());
        ATOM_CHECK(HasDiagnostic(outcome, "res://atlases/nope.atlas.json"));

        // No provider at all: the schema has no filesystem, so it says so rather than
        // inventing an empty sprite.
        const auto none = scene::ParseScene(Scene(R"({ "id": 1, "sprite": { "atlas": "res://atlases/terrain.atlas.json",
                                                                              "frame": "green" } })"),
                                            "res://scenes/test.scene.json");
        ATOM_CHECK(!none.Ok());
        ATOM_CHECK(HasDiagnostic(none, "no atlas provider"));
    }

    // --- 7. A bare texture keeps Linear and needs no atlas ----------------
    {
        const auto outcome = scene::ParseScene(Scene(R"({
            "id": 1,
            "sprite": { "texture": "res://textures/player.png", "size": [32.0, 32.0], "pivot": [0.5, 1.0] }
        })"),
                                               "res://scenes/test.scene.json", {}, provider);
        ATOM_CHECK(outcome.Ok());
        const auto& sprite = *outcome.scene->entities.front().sprite;
        ATOM_CHECK(sprite.texture_uri == "res://textures/player.png");
        ATOM_CHECK(!sprite.has_source);
        ATOM_CHECK(sprite.has_size);
        ATOM_CHECK(sprite.pivot.GetY() == 1.0F);
        // Non-sprite art is filtered linearly unless an atlas said otherwise.
        ATOM_CHECK(sprite.filter == ecs::SpriteFilter::Linear);
    }

    // --- 8. A prefab body may use an atlas frame too ----------------------
    //
    // The provider has to reach prefab parsing as well, otherwise "the crate sheet" would
    // work in a scene entity but not inside the prefab that most crates come from.
    {
        constexpr std::string_view kPrefab = R"({
            "schema_version": 1, "kind": "prefab", "id": "crate_atlas",
            "entity": {
                "sprite": { "atlas": "res://atlases/terrain.atlas.json", "frame": "green" }
            }
        })";
        const scene::PrefabProvider prefabs =
            [&provider](const std::string_view /*uri*/) -> Result<scene::EntityDesc> {
            // Reuse the same ParsePrefab path the loader uses, with the atlas provider.
            auto parsed_prefab = scene::ParsePrefab(kPrefab, "res://prefabs/crate_atlas.prefab.json", provider);
            if (!parsed_prefab.Ok())
                return std::unexpected<Error>{MakeError(ErrorCode::SceneInvalidField, Module::Scene, "test")
                                                  .WithDetail(parsed_prefab.diagnostics.front().Format())};
            return *parsed_prefab.entity;
        };

        const auto outcome = scene::ParseScene(Scene(R"({ "id": 1, "prefab": "res://prefabs/crate_atlas.prefab.json" })"),
                                               "res://scenes/test.scene.json", prefabs, provider);
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        const auto& sprite = *outcome.scene->entities.front().sprite;
        ATOM_CHECK(sprite.texture_uri == "res://textures/atlas.png");
        ATOM_CHECK(sprite.filter == ecs::SpriteFilter::Nearest);
        ATOM_CHECK(sprite.size.GetX() == 64.0F);
    }

    return 0;
}

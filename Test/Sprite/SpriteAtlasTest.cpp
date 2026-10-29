/**
 * @file           : SpriteAtlasTest.cpp
 * @brief          : Sprite atlas documents - parsing, defaults, and rejections.
 *
 * The atlas is the first layer of the art pipeline, so its job is narrow: turn one
 * sheet into named frames with a texel rect, a display size, a pivot, and one filter
 * for the whole sheet. The rules tested here are the ones that keep that single source
 * of truth intact - a frame without a rect has no meaning, a duplicate name would make
 * "the green cell" ambiguous, and a typo in `filter` must not silently fall back to
 * linear sampling of pixel art.
 */

#include <Sprite/SpriteAtlas.hpp>
#include <Sprite/SpriteAtlasLoader.hpp>

#include <Filesystem/FileSystem.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace {

using namespace atom;

constexpr std::string_view kValid = R"({
  // a comment, because the reader accepts them
  "schema_version": 1,
  "kind": "atlas",
  "id": "terrain",
  "texture": "res://textures/atlas.png",
  "filter": "nearest",
  "frames": {
    "red": { "rect": [0, 0, 8, 8] },
    "green": { "rect": [8, 0, 8, 8], "size": [64.0, 64.0] },
    "blue": { "rect": [0, 8, 8, 8], "pivot": [0.5, 1.0] }
  }
})";

auto HasDiagnostic(const sprite::AtlasParseOutcome& outcome, const std::string_view needle) -> bool {
    for (const auto& diagnostic : outcome.diagnostics) {
        if (diagnostic.Format().find(needle) != std::string::npos)
            return true;
    }
    return false;
}

// Runs a document through the parser and reports the first diagnostic, so a failing
// assertion says what actually went wrong.
auto Fails(const std::string_view text, const std::string_view expected) -> bool {
    const auto outcome = sprite::ParseAtlas(text, "res://atlases/test.atlas.json");
    if (outcome.Ok())
        return false;
    return HasDiagnostic(outcome, expected);
}

} // namespace

auto main() -> int {
    // --- 1. A valid atlas, its defaults and its order ----------------------
    {
        const auto outcome = sprite::ParseAtlas(kValid, "res://atlases/terrain.atlas.json");
        ATOM_CHECK_MSG(outcome.Ok(), outcome.Ok() ? "" : outcome.diagnostics.front().Format().c_str());
        const auto& atlas = *outcome.atlas;

        ATOM_CHECK(atlas.schema_version == 1);
        ATOM_CHECK(atlas.id == "terrain");
        ATOM_CHECK(atlas.texture_uri == "res://textures/atlas.png");
        ATOM_CHECK(atlas.filter == sprite::AtlasFilter::Nearest);
        ATOM_CHECK(atlas.frames.size() == 3);

        // Document order is preserved, because a diagnostic listing frames should read
        // the way the file reads.
        ATOM_CHECK(atlas.frames[0].name == "red");
        ATOM_CHECK(atlas.frames[1].name == "green");
        ATOM_CHECK(atlas.frames[2].name == "blue");

        // `red` sets nothing but the rect: display size defaults to the rect (1 texel =
        // 1 px) and the pivot defaults to the centre.
        const auto* red = atlas.Find("red");
        ATOM_CHECK(red != nullptr);
        ATOM_CHECK(red->source.x == 0.0F && red->source.y == 0.0F);
        ATOM_CHECK(red->source.width == 8.0F && red->source.height == 8.0F);
        ATOM_CHECK(red->size.GetX() == 8.0F && red->size.GetY() == 8.0F);
        ATOM_CHECK(red->pivot.GetX() == 0.5F && red->pivot.GetY() == 0.5F);

        // `green` overrides the display size, `blue` the pivot.
        const auto* green = atlas.Find("green");
        ATOM_CHECK(green != nullptr);
        ATOM_CHECK(green->source.x == 8.0F);
        ATOM_CHECK(green->size.GetX() == 64.0F && green->size.GetY() == 64.0F);
        const auto* blue = atlas.Find("blue");
        ATOM_CHECK(blue != nullptr);
        ATOM_CHECK(blue->pivot.GetY() == 1.0F);
        // The size default is applied after the rect, not after a later override.
        ATOM_CHECK(blue->size.GetX() == 8.0F);

        ATOM_CHECK(atlas.Find("nope") == nullptr);
        ATOM_CHECK(atlas.FrameNames() == "red, green, blue");
        ATOM_CHECK(atlas.FrameNames(2).find("1 more") != std::string::npos);
    }

    // --- 2. Filter vocabulary ---------------------------------------------
    {
        // Absent means linear: correct for non-sprite art, and the safer default for
        // anything that was authored without thinking about sampling.
        auto no_filter = std::string{kValid};
        no_filter.replace(no_filter.find("\"filter\": \"nearest\","), 22, "");
        const auto outcome = sprite::ParseAtlas(no_filter, "res://atlases/test.atlas.json");
        ATOM_CHECK(outcome.Ok());
        ATOM_CHECK(outcome.atlas->filter == sprite::AtlasFilter::Linear);

        // A near-miss is listed with the legal values instead of silently falling back:
        // "nearest-neighbour" and "point" are the names other tools use.
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x",
                               "texture": "res://t.png", "filter": "point",
                               "frames": { "a": { "rect": [0, 0, 1, 1] } } })",
                         "allowed: \"linear\", \"nearest\""));
    }

    // --- 3. Rejections -----------------------------------------------------
    {
        // Version and kind, for the same reasons a scene checks them.
        ATOM_CHECK(Fails(R"({ "schema_version": 2, "kind": "atlas", "id": "x",
                               "texture": "res://t.png", "frames": {} })",
                         "unsupported schema_version"));
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "scene", "id": "x",
                               "texture": "res://t.png", "frames": {} })",
                         "expected \"atlas\""));
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "id": "x", "texture": "res://t.png", "frames": {} })",
                         "\"kind\""));

        // The texture is the sheet, so it must be a mountable URI.
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x",
                               "texture": "textures/a.png", "frames": { "a": { "rect": [0, 0, 1, 1] } } })",
                         "res://"));
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x", "frames": {} })",
                         "requires a \"texture\""));

        // Frames need a rect, and a degenerate rect has no meaning.
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x",
                               "texture": "res://t.png", "frames": { "a": { "size": [8, 8] } } })",
                         "requires a \"rect\""));
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x",
                               "texture": "res://t.png", "frames": { "a": { "rect": [0, 0, 0, 8] } } })",
                         "greater than zero"));
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x",
                               "texture": "res://t.png", "frames": { "a": { "rect": [0, 0, 8, 8] } } })",
                         "") ==
                   false); // this one is valid

        // A frame may not invent fields; the suggestion is what makes a typo cheap.
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x",
                               "texture": "res://t.png", "frames": { "a": { "rect": [0, 0, 8, 8], "rec": [0, 0, 8, 8] } } })",
                         "did you mean \"rect\"?"));

        // An empty atlas is a mistake, not an empty sheet: nothing can be drawn.
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x",
                               "texture": "res://t.png", "frames": {} })",
                         "no frames"));

        // Duplicate frame names are caught by the JSONC reader, so "the green cell" can
        // never be ambiguous.
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "x", "texture": "res://t.png",
                               "frames": { "a": { "rect": [0, 0, 1, 1] }, "a": { "rect": [1, 1, 1, 1] } } })",
                         "duplicate key"));

        // Atlas ids are tooling names: lowercase, no spaces.
        ATOM_CHECK(Fails(R"({ "schema_version": 1, "kind": "atlas", "id": "Terrain Sheet",
                               "texture": "res://t.png", "frames": { "a": { "rect": [0, 0, 1, 1] } } })",
                         "may only contain"));
    }

    // --- 4. The shipped atlas is valid ------------------------------------
    //
    // This is the only place the repository's own atlas document is exercised, so a
    // broken sheet fails here instead of at the demo's boot.
    {
        std::unique_ptr<fs::NativeFileSystem> backend{};
        ATOM_CHECK(fs::NativeFileSystem::Create("res", std::string{ATOM_SOURCE_ROOT} + "/Example/Game/Assets",
                                                backend) == fs::Result::Success);

        sprite::SpriteAtlasLoader loader{*backend};
        const auto loaded = loader.Load("res://atlases/terrain.atlas.json");
        ATOM_CHECK_MSG(loaded.has_value(), loaded.has_value() ? "" : loaded.error().Message().c_str());
        const auto* atlas = *loaded;
        ATOM_CHECK(atlas != nullptr);
        ATOM_CHECK(atlas->id == "terrain");
        ATOM_CHECK(atlas->filter == sprite::AtlasFilter::Nearest);
        ATOM_CHECK(atlas->frames.size() == 4);

        const auto* green = atlas->Find("green");
        ATOM_CHECK(green != nullptr);
        ATOM_CHECK(green->source.x == 8.0F && green->source.width == 8.0F);
        ATOM_CHECK(green->size.GetX() == 64.0F);

        // Loading again returns the same object: the cache is what keeps a scene with
        // thirty frames from parsing the sheet thirty times.
        const auto again = loader.Load("res://atlases/terrain.atlas.json");
        ATOM_CHECK(again.has_value());
        ATOM_CHECK(*again == atlas);
        ATOM_CHECK(loader.LoadedCount() == 1);

        // A missing atlas is an Error carrying the uri, not a null atlas.
        const auto missing = loader.Load("res://atlases/nope.atlas.json");
        ATOM_CHECK(!missing.has_value());
        ATOM_CHECK(missing.error().Resource() == "res://atlases/nope.atlas.json");
    }

    return 0;
}

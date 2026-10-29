// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file SpriteAtlas.hpp
 * @brief A spritesheet's metadata: one texture, named frames, one filter.
 *
 * This is the art-pipeline's first layer and it exists for one reason: without it,
 * every sprite's texel rectangle, display size and pivot is hand-written into every
 * scene entity. A texture artist who slices a sheet into 8x8 cells should be able to
 * say "the green cell" and have the numbers live in one place.
 *
 * What a frame owns, and why:
 *   - `rect`  the texel rectangle (the only field the artist *must* provide);
 *   - `size`  display size in world pixels; defaults to the rect, so 1 texel = 1 px
 *             unless the author says otherwise. This is a per-frame art decision, not
 *             a per-entity one, which is what stops the same cell being drawn at
 *             three different sizes across a level;
 *   - `pivot` where the entity's position sits inside the sprite; same argument.
 *
 * `filter` is per atlas because it is a property of the *texture*, not of a draw call:
 * a pixel-art sheet must never be sampled with linear filtering, and one sheet is
 * usually all pixel art or all not.
 *
 * Frame names are object keys, so the JSONC reader's duplicate-key rejection is what
 * guarantees uniqueness - there is no separate check to forget.
 */

#ifndef ATOM_SPRITE_SPRITEATLAS_HPP
#define ATOM_SPRITE_SPRITEATLAS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Algorithm/Geometry/Rect.hpp>
#include <Algorithm/Vector/Vec2.hpp>
#include <Utilities/Json/Diagnostic.hpp>

namespace atom::sprite {

// Sampling for this sheet. Kept separate from any renderer enum because this is asset
// metadata; the layer that knows both (the scene loader) maps it.
enum class AtlasFilter : std::uint8_t { Linear, Nearest };

[[nodiscard]] auto FilterName(AtlasFilter filter) -> std::string_view;

struct AtlasFrame {
        std::string name{};
        // Texture-space rectangle in texels.
        algo::Rect source{};
        // World size in pixels; already defaulted to the rect's size at parse time, so
        // consumers never have to apply that rule twice.
        algo::Vec2 size{};
        algo::Vec2 pivot{0.5F, 0.5F};
        std::uint32_t line = 0;
        std::uint32_t column = 0;
};

struct SpriteAtlas {
        std::uint32_t schema_version = 1;
        std::string id{};
        std::string texture_uri{};
        AtlasFilter filter = AtlasFilter::Linear;
        // Insertion order is the document's order: it makes "the frames are ..." in a
        // diagnostic read the way the file reads.
        std::vector<AtlasFrame> frames{};
        std::unordered_map<std::string, std::size_t> frame_index{};

        [[nodiscard]] auto Find(std::string_view name) const -> const AtlasFrame*;
        // Comma-separated frame names, truncated for a one-line diagnostic.
        [[nodiscard]] auto FrameNames(std::size_t limit = 8) const -> std::string;
};

struct AtlasParseOutcome {
        std::optional<SpriteAtlas> atlas{};
        std::vector<json::Diagnostic> diagnostics{};

        [[nodiscard]] auto Ok() const noexcept -> bool {
            return atlas.has_value();
        }
};

// Current document version; an atlas with any other value is rejected.
inline constexpr std::uint32_t kSchemaVersion = 1;
inline constexpr std::size_t kMaxDiagnostics = 16;

// Parses `{ schema_version, kind:"atlas", id, texture, filter?, frames }`.
[[nodiscard]] auto ParseAtlas(std::string_view text, std::string_view uri) -> AtlasParseOutcome;

} // namespace atom::sprite

#endif // ATOM_SPRITE_SPRITEATLAS_HPP

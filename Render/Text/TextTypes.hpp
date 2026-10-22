/**
 * @file           : TextTypes.hpp
 * @brief          : Shared value types for the backend-neutral text pipeline.
 *
 * This is the frozen vocabulary of the Text subsystem: the stable font-instance
 * handle that replaces the raw Font* atlas key, the raster-representation
 * dimension that lets SDF/MSDF join later without re-migrating the atlas key,
 * the FreeType render mode that stays orthogonal to it, and the face-metrics
 * snapshot.
 *
 * W1 (contract freeze) — pure value types, no SDL_GPU / Vulkan / Renderer2D
 * dependency.
**/

#ifndef ATOM_RENDER_TEXT_TEXT_TYPES_HPP
#define ATOM_RENDER_TEXT_TEXT_TYPES_HPP

#include <cstdint>

namespace atom::text {

// Opaque, stable handle identifying one font instance (a concrete face at a
// concrete size/variation). Replaces the raw Font* that today keys the atlas
// and shaping. Invalid (value == 0) means "no instance".
struct FontInstanceId {
        std::uint32_t value = 0;

        [[nodiscard]] auto IsValid() const -> bool { return value != 0; }
        auto operator==(const FontInstanceId&) const -> bool = default;
        auto operator!=(const FontInstanceId&) const -> bool = default;
};

// Storage format of a rasterized glyph. This is the one dimension the old
// (Font*, size, codepoint) atlas key could not express, and the one that makes
// world/screen text (SDF/MSDF) a later *addition* rather than a re-migration.
//
// Note: a FontInstance rasterizer only ever *emits* the bitmap formats
// (AlphaBitmap / LCD / Mono / ColorBitmap). SDF and MSDF entries are produced
// by a separate generator (msdfgen at runtime, or an offline cook step) and
// share the same atlas-consumption path.
enum class RasterFormat : std::uint8_t {
    AlphaBitmap = 0, // 8-bit alpha (hinted, pixel-aligned UI text)
    LCD,             // subpixel RGB striped
    Mono,            // 1-bit
    SDF,             // signed distance field (screen-space px-range shader)
    MSDF,            // multi-channel signed distance field
    ColorBitmap,     // COLR / emoji (BGRA)
};

// FreeType rasterization mode. Orthogonal to RasterFormat: it selects *how* the
// outline is rendered (hinting / antialiasing style), not how the result is
// stored. It is ignored for SDF/MSDF/ColorBitmap, which are produced by a
// generator rather than FT_Render_Glyph.
enum class RenderMode : std::uint8_t {
    Normal = 0,
    Light,
    Mono,
    LCD,
};

// Face metrics at a given pixel height (px, top-left origin, y-down).
struct FontMetrics {
        float ascent = 0.0f;
        float descent = 0.0f;
        float line_gap = 0.0f;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_TEXT_TYPES_HPP

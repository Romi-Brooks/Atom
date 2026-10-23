/**
 * @file           : FontProvider.hpp
 * @brief          : Backend-neutral font family/instance/fallback contract.
 *
 * Owns font families, hands out stable instances, and resolves fallback chains.
 * Bundled fonts only — never fontconfig / CoreText / DWrite — so rendering is
 * reproducible across platforms and golden tests stay stable.
 *
 * W1 (contract freeze): the current FreeType single-face path (Render/Text/Font)
 * will be wrapped as FreeTypeFontProvider in W2 without changing this contract.
**/

#ifndef ATOM_RENDER_TEXT_FONT_PROVIDER_HPP
#define ATOM_RENDER_TEXT_FONT_PROVIDER_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include <Render/Text/TextTypes.hpp>

namespace atom::text {

// Raw rasterized glyph pixels, format-tagged so GlyphAtlas can route them into
// the right atlas page (alpha page, colour page, ...). `pixels` is packed
// according to `format` (width * height bytes for alpha, width * height * 4 for
// BGRA colour, ...).
struct GlyphRaster {
        RasterFormat format = RasterFormat::AlphaBitmap;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> pixels{};
        std::int32_t offset_x = 0; // bearing, raster px
        std::int32_t offset_y = 0;
};

// One live font instance. Implementations own the underlying FreeType face
// (and, later, per-worker clones for concurrent rasterization). The id stays
// stable for the instance's lifetime and is the shaping/atlas key.
class FontInstance {
    public:
        virtual ~FontInstance() = default;
        FontInstance(const FontInstance&) = delete;
        FontInstance& operator=(const FontInstance&) = delete;

        [[nodiscard]] virtual auto Id() const -> FontInstanceId = 0;
        [[nodiscard]] virtual auto HasGlyph(std::uint32_t codepoint) const -> bool = 0;
        // Maps a codepoint to its glyph id in this face, or 0 when absent. This
        // is the lookup the shaper needs from a font (HarfBuzz's get_glyph
        // callback) and the probe point for fallback detection.
        [[nodiscard]] virtual auto GlyphIndex(std::uint32_t codepoint) const -> std::uint32_t = 0;
        [[nodiscard]] virtual auto Metrics(float size_px) const -> FontMetrics = 0;
        [[nodiscard]] virtual auto ScaleForPixelHeight(float size_px) const -> float = 0;

        // Rasterize `glyph_id` at the given pixel size into `out`. Returns false
        // on a missing glyph or failure. `mode` selects hinting/AA style; the
        // emitted `out.format` is one of the bitmap formats (never SDF/MSDF).
        // Thread-safety: one instance is not shared across workers without a
        // per-worker clone (FreeType requires one face per concurrent user).
        [[nodiscard]] virtual auto Rasterize(std::uint32_t glyph_id, float size_px, RenderMode mode,
                                             GlyphRaster& out) const -> bool = 0;

    protected:
        FontInstance() = default;
};

// Owns font families and instances, and resolves fallback chains.
class FontProvider {
    public:
        virtual ~FontProvider() = default;
        FontProvider(const FontProvider&) = delete;
        FontProvider& operator=(const FontProvider&) = delete;

        // Creates (or reuses) an instance for `family` at `size_px`. Returns an
        // invalid id when the family cannot be resolved. `variation` is reserved
        // for OpenType variation axes (empty = default instance).
        [[nodiscard]] virtual auto CreateInstance(std::string_view family, float size_px,
                                                  std::string_view variation = {}) -> FontInstanceId = 0;

        // Next fallback instance that covers `codepoint`, or an invalid id when
        // the chain is exhausted. Called by the segmentation/shaper layer when a
        // run's font lacks a glyph.
        [[nodiscard]] virtual auto FallbackFor(FontInstanceId instance, std::uint32_t codepoint) -> FontInstanceId = 0;

        [[nodiscard]] virtual auto GetInstance(FontInstanceId id) -> FontInstance* = 0;
        [[nodiscard]] virtual auto GetInstance(FontInstanceId id) const -> const FontInstance* = 0;

    protected:
        FontProvider() = default;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_FONT_PROVIDER_HPP

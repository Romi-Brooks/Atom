/**
 * @file           : GlyphAtlas.hpp
 * @brief          : Backend-neutral glyph atlas contract.
 *
 * Rasterizes glyphs (via a FontProvider instance) and packs them into pages,
 * exposing placement + page residency for upload. Upload and eviction are
 * driven by the render thread through explicit calls; this type never touches
 * SDL_GPU / Vulkan. The render backend resolves the opaque page index to a
 * texture at draw time.
 *
 * W1 (contract freeze): the key carries the raster-format dimension as a
 * first-class field (not folded into render_mode), so SDF/MSDF joins later as
 * "a new provider + a new atlas page + a new shader", not a second migration.
**/

#ifndef ATOM_RENDER_TEXT_GLYPH_ATLAS_HPP
#define ATOM_RENDER_TEXT_GLYPH_ATLAS_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <Render/Text/TextTypes.hpp>

namespace atom::text {

// Full identity of one rasterized glyph in the atlas.
struct GlyphKey {
        FontInstanceId font_instance;
        std::uint32_t glyph_id = 0;
        float raster_px = 0.0f; // quantized pixel size
        RasterFormat format = RasterFormat::AlphaBitmap;
        RenderMode render_mode = RenderMode::Normal;

        auto operator==(const GlyphKey&) const -> bool = default;
};

struct GlyphKeyHash {
        auto operator()(const GlyphKey& key) const noexcept -> std::size_t {
                std::size_t h = std::hash<std::uint32_t>{}(key.font_instance.value);
                h ^= std::hash<std::uint32_t>{}(key.glyph_id) + 0x9e3779b9u + (h << 6) + (h >> 2);
                h ^= std::hash<float>{}(key.raster_px) + 0x9e3779b9u + (h << 6) + (h >> 2);
                h ^= std::hash<int>{}(static_cast<int>(key.format)) + 0x9e3779b9u + (h << 6) + (h >> 2);
                h ^= std::hash<int>{}(static_cast<int>(key.render_mode)) + 0x9e3779b9u + (h << 6) + (h >> 2);
                return h;
        }
};

// Where a glyph ended up in the atlas. `page` is an opaque index the render
// backend maps to a texture; UV is normalized to [0, 1] within that page.
struct GlyphPlacement {
        std::uint32_t page = 0;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        float width = 0.0f;  // raster px
        float height = 0.0f; // raster px
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        RasterFormat format = RasterFormat::AlphaBitmap;
};

// A dirty sub-region of a page, ready to upload on the render thread at a frame
// boundary (see W0 upload queue). Backend-neutral: pixels only, no GPU handles.
struct AtlasDirtyRegion {
        std::uint32_t page = 0;
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> pixels{}; // width * height * 4, straight RGBA
};

// Backend-neutral glyph atlas manager.
class GlyphAtlas {
    public:
        virtual ~GlyphAtlas() = default;
        GlyphAtlas(const GlyphAtlas&) = delete;
        GlyphAtlas& operator=(const GlyphAtlas&) = delete;

        // Ensure the glyph is resident; returns a placement with page == invalid
        // when the glyph cannot be rasterized (missing glyph with no .notdef).
        [[nodiscard]] virtual auto Ensure(const GlyphKey& key) -> GlyphPlacement = 0;

        [[nodiscard]] virtual auto PageCount() const -> std::uint32_t = 0;

        // Drains pending page uploads. Render-thread-only; called at a frame
        // boundary. The backend uploads each region and reports success so the
        // atlas can mark the page clean.
        [[nodiscard]] virtual auto TakeDirtyRegions() -> std::vector<AtlasDirtyRegion> = 0;

        // Eviction / DPI budget. Declared now, exercised in W4 (LRU over pages).
        virtual auto SetByteBudget(std::size_t bytes) -> void = 0;
        virtual auto Compact() -> void = 0;

    protected:
        GlyphAtlas() = default;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_GLYPH_ATLAS_HPP

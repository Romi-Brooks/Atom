/**
 * @file           : FreeTypeFontProvider.hpp
 * @brief          : FreeType-backed FontProvider (W2a).
 *
 * Bundled font bytes are registered per family; each FontInstance owns a
 * FreeType face rasterized on demand. This is the first concrete FontProvider,
 * and it deliberately keeps the old codepoint-based `Font` (Render/Text/Font.hpp)
 * untouched: that path is retired in W4, when the atlas key upgrades from
 * (Font*, size, codepoint) to (FontInstanceId, glyph_id, ...).
 *
 * Thread model: a single face per instance guarded by a mutex (FreeType forbids
 * concurrent use of one face). W2b adds per-worker face clones so rasterization
 * can run on the job system without that lock.
**/

#ifndef ATOM_RENDER_TEXT_FREETYPE_FONT_PROVIDER_HPP
#define ATOM_RENDER_TEXT_FREETYPE_FONT_PROVIDER_HPP

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <Render/Text/FontProvider.hpp>

namespace atom::text {

// One glyph to rasterize. `instance` resolves the family bytes; `size_px` and
// `mode` are the rasterization parameters. Workers clone their own FT_Library +
// FT_Face from the (read-only) family bytes, so no shared face is touched
// concurrently.
struct RasterizeRequest {
        FontInstanceId instance;
        std::uint32_t glyph_id = 0;
        float size_px = 16.0f;
        RenderMode mode = RenderMode::Normal;
};

// Result of one rasterize request. `ok` is false for an invalid instance, a
// missing/out-of-range glyph, or a FreeType failure; `raster` is then empty.
struct RasterizeResult {
        bool ok = false;
        GlyphRaster raster{};
};

class FreeTypeFontProvider final : public FontProvider {
    public:
        FreeTypeFontProvider();
        ~FreeTypeFontProvider() override;

        FreeTypeFontProvider(const FreeTypeFontProvider&) = delete;
        FreeTypeFontProvider& operator=(const FreeTypeFontProvider&) = delete;

        // Registers a family's font bytes (copied). Returns false on empty bytes
        // or a face FreeType cannot parse. Call before CreateInstance.
        [[nodiscard]] auto AddFamily(std::string_view family, std::span<const std::byte> bytes) -> bool;

        // --- FontProvider ----------------------------------------------------
        [[nodiscard]] auto CreateInstance(std::string_view family, float size_px, std::string_view variation = {})
            -> FontInstanceId override;
        [[nodiscard]] auto FallbackFor(FontInstanceId instance, std::uint32_t codepoint) -> FontInstanceId override;
        [[nodiscard]] auto GetInstance(FontInstanceId id) -> FontInstance* override;
        [[nodiscard]] auto GetInstance(FontInstanceId id) const -> const FontInstance* override;

        // --- FreeType-specific surface (for HarfBuzzTextShaper) --------------
        // Returns a span over the face's backing font file bytes. Valid for the
        // instance's lifetime; empty when `id` is invalid. The shaper feeds this
        // to HarfBuzz so the real cmap/GSUB/GPOS tables drive shaping, without
        // exposing an FT_Face across the backend-neutral contract.
        [[nodiscard]] auto BytesFor(FontInstanceId id) const -> std::span<const std::byte>;
        // Pixel size the instance was created at (the shaper's hb_font scale).
        [[nodiscard]] auto SizePxFor(FontInstanceId id) const -> float;

        // Rasterizes a batch in parallel through the job system: one job per
        // request, each cloning its own FT_Library + FT_Face from the family
        // bytes (no shared face, no lock). Results are returned in input order.
        // Falls back to sequential rasterization when the job system is not
        // running. Blocks until all requests finish (the calling thread helps
        // run pending jobs, matching AudioMetadataReader::ReadBatch).
        [[nodiscard]] auto RasterizeBatch(std::span<const RasterizeRequest> requests) -> std::vector<RasterizeResult>;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_FREETYPE_FONT_PROVIDER_HPP

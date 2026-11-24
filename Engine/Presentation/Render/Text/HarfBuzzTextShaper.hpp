/**
 * @file           : HarfBuzzTextShaper.hpp
 * @brief          : HarfBuzz-backed TextShaper (W2c), first concrete shaper.
 *
 * Shapes one UTF-8 run under a single FreeType font instance into glyph ids,
 * clusters and advances. It drives the real OpenType tables (cmap / GSUB /
 * GPOS) by handing HarfBuzz the instance's font file bytes, so kerning,
 * ligatures and mark positioning come for free without exposing an FT_Face
 * across the backend-neutral contract.
 *
 * Scope (W2c): Latin + CJK + combining marks. RTL / Arabic / Indic / emoji are
 * later increments; the caller (segmentation layer) is responsible for
 * splitting text into per-script / per-fallback runs before Shape().
 *
 * Thread model: Shape() is pure CPU and uses no shared mutable state beyond the
 * provider (read-only byte lookup), so it is worker-safe as-is. A run-level
 * cache and job-ized shaping land with W2's async pass.
**/

#ifndef ATOM_RENDER_TEXT_HARFBUZZ_TEXT_SHAPER_HPP
#define ATOM_RENDER_TEXT_HARFBUZZ_TEXT_SHAPER_HPP

#include <Render/Text/FreeTypeFontProvider.hpp>
#include <Render/Text/TextShaper.hpp>

namespace atom::text {

class HarfBuzzTextShaper final : public TextShaper {
    public:
        // The provider must outlive this shaper; only its read-only byte lookup
        // is used during Shape().
        explicit HarfBuzzTextShaper(const FreeTypeFontProvider& provider);

        ~HarfBuzzTextShaper() override;

        HarfBuzzTextShaper(const HarfBuzzTextShaper&) = delete;
        HarfBuzzTextShaper& operator=(const HarfBuzzTextShaper&) = delete;

        [[nodiscard]] auto Shape(FontInstanceId instance, std::string_view utf8,
                                 const ShapeParams& params = {}) const -> ShapedRun override;

    private:
        struct Impl;
        Impl* impl_;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_HARFBUZZ_TEXT_SHAPER_HPP

/**
 * @file           : TextShaper.hpp
 * @brief          : Backend-neutral shaping contract (HarfBuzz adapter lands in W2).
 *
 * Turns UTF-8 text + a resolved font instance into glyph runs. Pure CPU, so a
 * conforming implementation can be scheduled on a worker and cached per
 * (instance, text, params).
 *
 * The first implementation (HarfBuzzTextShaper) covers Latin + CJK + combining
 * marks; RTL/Arabic/Indic/emoji are later increments. The caller (the Unicode
 * segmentation layer) is responsible for splitting text into per-script and
 * per-fallback runs before calling Shape().
**/

#ifndef ATOM_RENDER_TEXT_TEXT_SHAPER_HPP
#define ATOM_RENDER_TEXT_TEXT_SHAPER_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include <Render/Text/TextTypes.hpp>

namespace atom::text {

// One shaped glyph as produced by the shaper, before layout positioning. The
// cluster is a source-byte offset into the original UTF-8 — the basis for
// caret placement, hit-testing, selection and IME anchoring.
struct ShapedGlyph {
        FontInstanceId font_instance;
        std::uint32_t glyph_id = 0;
        std::uint32_t cluster_byte_offset = 0;
        float x_advance = 0.0f;
        float y_advance = 0.0f;
        float x_offset = 0.0f;
        float y_offset = 0.0f;
};

// A shaped run under a single font instance (one script / one fallback step).
struct ShapedRun {
        FontInstanceId font_instance;
        std::vector<ShapedGlyph> glyphs{};
};

// Direction/script/language/features inputs. Reserved so a real
// HarfBuzzTextShaper can pass them straight through; the first implementation
// treats them as hints (Latin + CJK + combining marks only).
struct ShapeParams {
        std::string_view direction = "ltr"; // "ltr" | "rtl" | "ttb" | "btt"
        std::string_view script = {};       // ISO 15924 ("" = auto)
        std::string_view language = {};     // BCP-47 ("" = auto)
        std::string_view features = {};     // OpenType feature list, shaper-defined
};

// Shapes one run under a single font instance. Pure CPU, worker-safe.
class TextShaper {
    public:
        virtual ~TextShaper() = default;
        TextShaper(const TextShaper&) = delete;
        TextShaper& operator=(const TextShaper&) = delete;

        [[nodiscard]] virtual auto Shape(FontInstanceId instance, std::string_view utf8,
                                         const ShapeParams& params = {}) -> ShapedRun = 0;

    protected:
        TextShaper() = default;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_TEXT_SHAPER_HPP

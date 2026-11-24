/**
 * @file           : TextLayout.hpp
 * @brief          : Immutable text layout data shape.
 *
 * The result of laying out UTF-8 text: positioned glyphs, line boxes, and the
 * source-byte -> cluster mapping used for caret / hit-test / selection. Built
 * once and held by the caller, so static UI never re-lays-out per frame.
 *
 * W1 (contract freeze): this header freezes only the data shape so TextRenderer
 * and UI can consume it before the builder (W3) lands. Construction goes
 * through TextLayoutBuilder (W3), which needs a shaper + segmentation.
**/

#ifndef ATOM_RENDER_TEXT_TEXT_LAYOUT_HPP
#define ATOM_RENDER_TEXT_TEXT_LAYOUT_HPP

#include <cstdint>
#include <span>
#include <vector>

#include <Render/Text/TextTypes.hpp>

namespace atom::text {

class TextLayoutBuilder;

// A single glyph's laid-out pen origin, relative to the layout's top-left.
struct PositionedGlyph {
        FontInstanceId font_instance;
        std::uint32_t glyph_id = 0;
        std::uint32_t cluster_byte_offset = 0;
        float x = 0.0f;
        float y = 0.0f;
        float x_advance = 0.0f;
        float y_advance = 0.0f;
};

// One laid-out line. `glyphs` views into the owning TextLayout's glyph storage.
struct TextLine {
        std::span<const PositionedGlyph> glyphs{};
        float baseline = 0.0f;
        float width = 0.0f;
        float ascent = 0.0f;
        float descent = 0.0f;
};

// Immutable layout result. Move-only: lines reference internal glyph storage,
// and the spans stay valid across a move (vector buffer ownership transfers).
class TextLayout {
    public:
        TextLayout() = default;
        TextLayout(TextLayout&&) noexcept = default;
        auto operator=(TextLayout&&) noexcept -> TextLayout& = default;
        TextLayout(const TextLayout&) = delete;
        auto operator=(const TextLayout&) = delete;

        [[nodiscard]] auto Lines() const -> std::span<const TextLine> { return lines_; }
        [[nodiscard]] auto Width() const -> float { return width_; }
        [[nodiscard]] auto Height() const -> float { return height_; }

    private:
        friend class TextLayoutBuilder;

        std::vector<PositionedGlyph> glyphs_{};
        std::vector<TextLine> lines_{};
        float width_ = 0.0f;
        float height_ = 0.0f;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_TEXT_LAYOUT_HPP

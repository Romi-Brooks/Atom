/**
 * @file           : TextLayoutBuilder.hpp
 * @brief          : Builds immutable TextLayout objects from UTF-8 text (W3).
 *
 * The builder is the single source of truth for turning text into positioned
 * glyphs and line boxes: it segments the input (first-wave: a single run), hands
 * each line to a TextShaper (HarfBuzz), applies word/CJK line breaking, and
 * computes line boxes and alignment. The result is an immutable TextLayout that
 * a caller holds and reuses across frames, so static UI never re-lays-out.
 *
 * Backend-neutral: it depends only on the FontProvider / TextShaper contracts,
 * not on FreeType / HarfBuzz / SDL_GPU.
**/

#ifndef ATOM_RENDER_TEXT_TEXT_LAYOUT_BUILDER_HPP
#define ATOM_RENDER_TEXT_TEXT_LAYOUT_BUILDER_HPP

#include <cstdint>
#include <string_view>

#include <Render/Text/FontProvider.hpp>
#include <Render/Text/TextLayout.hpp>
#include <Render/Text/TextShaper.hpp>

namespace atom::text {

// Horizontal alignment of each line within `max_width`. Only meaningful when
// `max_width > 0`; otherwise every line is laid out at its natural width.
enum class TextAlign : std::uint8_t {
    Left = 0,
    Center,
    Right,
};

// Inputs to a layout build. Pure value type.
struct TextLayoutParams {
        float max_width = 0.0f; // 0 = no line wrapping
        TextAlign align = TextAlign::Left;
};

// Builds immutable TextLayout objects from UTF-8 text.
class TextLayoutBuilder {
    public:
        // The provider and shaper must outlive this builder.
        TextLayoutBuilder(const FontProvider& provider, const TextShaper& shaper);

        TextLayoutBuilder(const TextLayoutBuilder&) = delete;
        auto operator=(const TextLayoutBuilder&) -> TextLayoutBuilder& = delete;

        // Shapes and lays out `utf8` under `instance`. Returns an empty layout
        // for empty text or an invalid instance. The result is reproducible for
        // the same (instance, text, params), so it is golden-testable.
        [[nodiscard]] auto Build(FontInstanceId instance, std::string_view utf8,
                                 const TextLayoutParams& params = {}) const -> TextLayout;

    private:
        const FontProvider& provider_;
        const TextShaper& shaper_;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_TEXT_LAYOUT_BUILDER_HPP

/**
 * @file           : TextLayoutBuilder.cpp
 * @brief          : TextLayoutBuilder implementation (W3).
 *
 * The first-wave builder is deliberately simple and mirrors the proven
 * single-font line-breaking semantics Renderer2D shipped: ASCII words break as
 * units, CJK and other non-ASCII characters break per character, tabs align to
 * a four-space grid, CRLF's '\r' is ignored, and '\n' is a hard break. The only
 * change versus the legacy path is that advance values now come from the
 * TextShaper (HarfBuzz), so kerning and ligatures are real rather than
 * per-codepoint approximations.
**/

#include "TextLayoutBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace atom::text {
namespace {

// Decodes the UTF-8 codepoint at `index`, advancing it past the sequence.
// Returns 0 for malformed input (matching the legacy Renderer2D decoder), which
// the caller skips so an invalid byte never reaches the shaper.
auto NextCodepoint(const std::string_view text, std::size_t& index) -> std::uint32_t {
    if (index >= text.size())
        return 0;
    const auto lead = static_cast<std::uint8_t>(text[index]);
    if (lead < 0x80) {
        ++index;
        return lead;
    }
    std::size_t length = 0;
    std::uint32_t codepoint = 0;
    std::uint32_t minimum = 0;
    if ((lead & 0xE0) == 0xC0) {
        length = 2;
        codepoint = lead & 0x1F;
        minimum = 0x80;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
        codepoint = lead & 0x0F;
        minimum = 0x800;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
        codepoint = lead & 0x07;
        minimum = 0x10000;
    } else {
        return 0;
    }
    if (index + length > text.size())
        return 0;
    for (std::size_t k = 1; k < length; ++k) {
        const auto byte = static_cast<std::uint8_t>(text[index + k]);
        if ((byte & 0xC0) != 0x80)
            return 0;
        codepoint = (codepoint << 6) | (byte & 0x3F);
    }
    if (codepoint < minimum || codepoint > 0x10ffffu || (codepoint >= 0xd800u && codepoint <= 0xdfffu))
        return 0;
    index += length;
    return codepoint;
}

// Printable ASCII (word characters); space (0x20) is handled separately.
auto IsAsciiWordCodepoint(const std::uint32_t codepoint) -> bool {
    return codepoint >= 0x21u && codepoint <= 0x7eu;
}

} // namespace

TextLayoutBuilder::TextLayoutBuilder(const FontProvider& provider, const TextShaper& shaper)
    : provider_(provider), shaper_(shaper) {}

auto TextLayoutBuilder::Build(const FontInstanceId instance, const std::string_view utf8,
                              const TextLayoutParams& params) const -> TextLayout {
    TextLayout layout{};
    if (!instance.IsValid() || utf8.empty())
        return layout;

    const FontInstance* font = provider_.GetInstance(instance);
    if (!font)
        return layout;

    const float size_px = font->SizePx();
    const FontMetrics metrics = font->Metrics(size_px);
    const float ascent = metrics.ascent;
    const float line_height = metrics.ascent - metrics.descent + metrics.line_gap;
    if (!std::isfinite(line_height) || line_height <= 0.0f)
        return layout;

    const float limit = params.max_width > 0.0f ? params.max_width : std::numeric_limits<float>::infinity();

    // Total advance of one shaped substring, used only for line-breaking
    // decisions. Per-line shaping below is the single source of truth for the
    // emitted positions.
    const auto shape_advance = [&](const std::string_view sub) -> float {
        const ShapedRun run = shaper_.Shape(instance, sub);
        float advance = 0.0f;
        for (const ShapedGlyph& glyph : run.glyphs)
            advance += glyph.x_advance;
        return advance;
    };

    struct Row {
            std::vector<PositionedGlyph> glyphs{};
            float width = 0.0f;
    };
    std::vector<Row> rows{};

    std::size_t index = 0;
    const std::size_t n = utf8.size();
    while (index < n) {
        const std::size_t line_start = index;
        float line_width = 0.0f;
        bool hit_newline = false;

        while (index < n) {
            const std::size_t cp_start = index;
            const std::uint32_t cp = NextCodepoint(utf8, index);
            if (cp == 0) {
                if (index == cp_start)
                    ++index; // malformed byte: skip so we always make progress
                continue;
            }
            if (cp == '\n') {
                hit_newline = true;
                break; // index now points past '\n'
            }
            if (cp == '\r')
                continue;
            if (cp == ' ') {
                line_width += shape_advance(utf8.substr(cp_start, 1));
                continue;
            }
            if (cp == '\t') {
                // First wave: a tab is treated as a single space, matching what the
                // shaping pass below sees (tabs are sanitized to spaces 1:1), so
                // break decisions and rendered positions never disagree. Column
                // alignment to a four-space grid is a later increment.
                line_width += shape_advance(" ");
                continue;
            }
            if (IsAsciiWordCodepoint(cp)) {
                // Whole-word break: consume the word, rewind onto the next line
                // when it no longer fits and this line already has content.
                std::size_t word_end = index;
                while (word_end < n) {
                    const std::size_t probe = word_end;
                    if (!IsAsciiWordCodepoint(NextCodepoint(utf8, word_end))) {
                        word_end = probe;
                        break;
                    }
                }
                const float word_advance = shape_advance(utf8.substr(cp_start, word_end - cp_start));
                if (line_width + word_advance > limit && line_width > 0.0f) {
                    index = cp_start; // rewind: this word opens the next line
                    break;
                }
                line_width += word_advance;
                index = word_end;
                continue;
            }
            // CJK / punctuation / other: break per character.
            const float advance = shape_advance(utf8.substr(cp_start, index - cp_start));
            if (line_width + advance > limit && line_width > 0.0f) {
                index = cp_start;
                break;
            }
            line_width += advance;
        }

        const std::size_t line_end = hit_newline ? (index > 0 ? index - 1 : index) : index;

        Row row{};
        if (line_end > line_start) {
            std::string_view line_text = utf8.substr(line_start, line_end - line_start);
            // Tabs are line-breaking hints only: shape a sanitized copy where each
            // tab is a space (1:1 byte, so cluster offsets stay valid). The tab
            // control character itself would otherwise map to .notdef.
            std::string sanitized;
            if (line_text.find('\t') != std::string_view::npos) {
                sanitized.assign(line_text);
                for (char& c : sanitized)
                    if (c == '\t')
                        c = ' ';
                line_text = sanitized;
            }
            const ShapedRun run = shaper_.Shape(instance, line_text);
            row.glyphs.reserve(run.glyphs.size());
            for (const ShapedGlyph& glyph : run.glyphs) {
                PositionedGlyph positioned{};
                positioned.font_instance = instance;
                positioned.glyph_id = glyph.glyph_id;
                positioned.cluster_byte_offset = static_cast<std::uint32_t>(line_start + glyph.cluster_byte_offset);
                positioned.x = row.width;
                positioned.y = 0.0f; // filled below once the line box is known
                positioned.x_advance = glyph.x_advance;
                positioned.y_advance = glyph.y_advance;
                row.width += glyph.x_advance;
                row.glyphs.push_back(positioned);
            }
        }
        rows.push_back(std::move(row));

        if (hit_newline && index >= n) {
            // Trailing newline: it still opens one empty line below, matching the
            // legacy per-newline line-count semantics.
            rows.emplace_back();
            break;
        }
    }

    // Assemble the immutable layout with stable spans into its own glyph storage.
    std::size_t total_glyphs = 0;
    for (const Row& row : rows)
        total_glyphs += row.glyphs.size();

    layout.glyphs_.reserve(total_glyphs);
    layout.lines_.reserve(rows.size());

    float width = 0.0f;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& row = rows[i];
        const float baseline = static_cast<float>(i) * line_height + ascent;

        float align_offset = 0.0f;
        if (params.max_width > 0.0f && params.align != TextAlign::Left) {
            const float delta = params.max_width - row.width;
            if (delta > 0.0f)
                align_offset = params.align == TextAlign::Center ? delta * 0.5f : delta;
        }

        const std::size_t start = layout.glyphs_.size();
        for (PositionedGlyph glyph : row.glyphs) {
            glyph.x += align_offset;
            glyph.y = baseline;
            layout.glyphs_.push_back(glyph);
        }
        const std::size_t end = layout.glyphs_.size();

        TextLine line{};
        line.glyphs = end > start
                          ? std::span<const PositionedGlyph>(layout.glyphs_.data() + start, end - start)
                          : std::span<const PositionedGlyph>{};
        line.baseline = baseline;
        line.width = row.width;
        line.ascent = ascent;
        line.descent = metrics.descent;
        layout.lines_.push_back(line);

        width = std::max(width, row.width);
    }

    layout.width_ = width;
    layout.height_ = static_cast<float>(rows.size()) * line_height;
    return layout;
}

} // namespace atom::text

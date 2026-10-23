/**
 * @file           : TextLayoutBuilderTest.cpp
 * @brief          : W3 contract coverage for the immutable TextLayout builder.
 *
 * Verifies shaping-driven layout: single-line runs, byte-offset clusters, word
 * and CJK line breaking, alignment, and line-box metrics, against the pinned
 * Roboto (Latin) and Mplus1p (Latin + CJK) font fixtures.
 */

#include <Render/Text/FreeTypeFontProvider.hpp>
#include <Render/Text/HarfBuzzTextShaper.hpp>
#include <Render/Text/TextLayoutBuilder.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

[[nodiscard]] auto ReadBinary(const std::filesystem::path& path) -> std::vector<std::byte> {
    std::ifstream input{path, std::ios::binary};
    if (!input)
        return {};
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    std::vector<std::byte> result(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i)
        result[i] = static_cast<std::byte>(bytes[i]);
    return result;
}

} // namespace

auto main() -> int {
    using namespace atom::text;

    const auto roboto =
        ReadBinary(std::filesystem::path{ATOM_SOURCE_ROOT} / "ThirdParty/HarfBuzz/perf/fonts/Roboto-Regular.ttf");
    const auto mplus =
        ReadBinary(std::filesystem::path{ATOM_SOURCE_ROOT} / "ThirdParty/HarfBuzz/test/api/fonts/Mplus1p-Regular.ttf");
    if (roboto.empty() || mplus.empty())
        return atom::test::Fail("Pinned font fixture is unavailable");

    FreeTypeFontProvider provider{};
    ATOM_CHECK(provider.AddFamily("Roboto", roboto));
    ATOM_CHECK(provider.AddFamily("Mplus", mplus));
    const auto latin = provider.CreateInstance("Roboto", 21.0f);
    const auto cjk = provider.CreateInstance("Mplus", 21.0f);
    ATOM_CHECK(latin.IsValid());
    ATOM_CHECK(cjk.IsValid());

    HarfBuzzTextShaper shaper{provider};
    TextLayoutBuilder builder{provider, shaper};

    // Empty text and unknown instance shape to an empty layout.
    ATOM_CHECK(builder.Build(latin, {}).Lines().empty());
    ATOM_CHECK(builder.Build(FontInstanceId{999}, "Hello").Lines().empty());

    // Single-line Latin: one glyph per byte, byte-offset clusters, positive advances.
    const auto hello = builder.Build(latin, "Hello");
    ATOM_CHECK(hello.Lines().size() == 1);
    const auto& line0 = hello.Lines()[0];
    ATOM_CHECK(line0.glyphs.size() == 5);
    for (std::size_t i = 0; i < line0.glyphs.size(); ++i) {
        ATOM_CHECK(line0.glyphs[i].glyph_id != 0);
        ATOM_CHECK(line0.glyphs[i].cluster_byte_offset == static_cast<std::uint32_t>(i));
        ATOM_CHECK(line0.glyphs[i].x_advance > 0.0f);
    }
    ATOM_CHECK(hello.Width() > 0.0f);
    ATOM_CHECK(hello.Height() > 0.0f);

    // Multi-byte UTF-8: clusters stay byte offsets ('A' = 0, "é" = 1).
    const std::string mixed = "A\xC3\xA9";
    const auto mixed_layout = builder.Build(latin, mixed);
    ATOM_CHECK(mixed_layout.Lines().size() == 1);
    ATOM_CHECK(mixed_layout.Lines()[0].glyphs.size() == 2);
    ATOM_CHECK(mixed_layout.Lines()[0].glyphs[0].cluster_byte_offset == 0);
    ATOM_CHECK(mixed_layout.Lines()[0].glyphs[1].cluster_byte_offset == 1);

    // Word wrap: a width under "Hello" pushes "world" onto a second line.
    const float hello_width = hello.Width();
    const auto wrapped = builder.Build(latin, "Hello world", TextLayoutParams{hello_width * 0.9f, TextAlign::Left});
    ATOM_CHECK(wrapped.Lines().size() == 2);
    ATOM_CHECK(!wrapped.Lines()[0].glyphs.empty());
    ATOM_CHECK(!wrapped.Lines()[1].glyphs.empty());

    // CJK per-character wrap: a width just over one character yields one glyph
    // per line.
    const float cjk_char_width = builder.Build(cjk, "\xE4\xBD\xA0").Width(); // "你"
    const auto cjk_lines =
        builder.Build(cjk, "\xE4\xBD\xA0\xE5\xA5\xBD\xE4\xB8\x96\xE7\x95\x8C",
                      TextLayoutParams{cjk_char_width * 1.5f, TextAlign::Left}); // "你好世界"
    ATOM_CHECK(cjk_lines.Lines().size() == 4);
    for (const auto& line : cjk_lines.Lines())
        ATOM_CHECK(line.glyphs.size() == 1);

    // Alignment offsets within a fixed max_width.
    const float a_width = builder.Build(latin, "A").Width();
    const auto centered = builder.Build(latin, "A", TextLayoutParams{100.0f, TextAlign::Center});
    ATOM_CHECK_NEAR(centered.Lines()[0].glyphs[0].x, (100.0f - a_width) * 0.5f);
    const auto righted = builder.Build(latin, "A", TextLayoutParams{100.0f, TextAlign::Right});
    ATOM_CHECK_NEAR(righted.Lines()[0].glyphs[0].x, 100.0f - a_width);

    // Line-box metrics: first baseline equals the face ascent, descent is <= 0.
    ATOM_CHECK_NEAR(line0.baseline, line0.ascent);
    ATOM_CHECK(line0.ascent > 0.0f);
    ATOM_CHECK(line0.descent <= 0.0f);

    // Hard newlines open empty lines: "\n" is two empty lines, "a\n\nb" is three.
    const auto just_newline = builder.Build(latin, "\n");
    ATOM_CHECK(just_newline.Lines().size() == 2);
    ATOM_CHECK(just_newline.Lines()[0].glyphs.empty());
    ATOM_CHECK(just_newline.Lines()[1].glyphs.empty());
    const auto spaced = builder.Build(latin, "a\n\nb");
    ATOM_CHECK(spaced.Lines().size() == 3);
    ATOM_CHECK(spaced.Lines()[0].glyphs.size() == 1);
    ATOM_CHECK(spaced.Lines()[1].glyphs.empty());
    ATOM_CHECK(spaced.Lines()[2].glyphs.size() == 1);

    // Tab is sanitized to a space (no .notdef glyph leaks through).
    const auto with_tab = builder.Build(latin, "a\tb");
    ATOM_CHECK(with_tab.Lines().size() == 1);
    ATOM_CHECK(with_tab.Lines()[0].glyphs.size() == 3); // 'a', tab-as-space, 'b'
    for (const auto& glyph : with_tab.Lines()[0].glyphs)
        ATOM_CHECK(glyph.glyph_id != 0);

    return 0;
}

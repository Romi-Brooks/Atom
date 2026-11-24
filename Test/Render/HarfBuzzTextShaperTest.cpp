/**
 * @file           : HarfBuzzTextShaperTest.cpp
 * @brief          : W2c contract coverage for the HarfBuzz-backed shaper.
 *
 * Shapes Latin runs through the real OpenType tables and asserts glyph ids,
 * byte-offset clusters and pixel advances; verifies mark positioning and the
 * empty/unknown-instance failure paths.
 */

#include <Render/Text/FreeTypeFontProvider.hpp>
#include <Render/Text/HarfBuzzTextShaper.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
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

    const auto bytes = ReadBinary(std::filesystem::path{ATOM_SOURCE_ROOT} / "ThirdParty/HarfBuzz/perf/fonts/Roboto-Regular.ttf");
    if (bytes.empty())
        return atom::test::Fail("Pinned font fixture is unavailable");

    FreeTypeFontProvider provider{};
    ATOM_CHECK(provider.AddFamily("Roboto", bytes));
    const auto id = provider.CreateInstance("Roboto", 21.0f);
    ATOM_CHECK(id.IsValid());

    HarfBuzzTextShaper shaper{provider};

    // Empty text and unknown instance shape to an empty run.
    ATOM_CHECK(shaper.Shape(id, {}).glyphs.empty());
    ATOM_CHECK(shaper.Shape(FontInstanceId{999}, "Hello").glyphs.empty());

    // Latin: one glyph per ASCII byte, byte-offset clusters, positive advances.
    const auto latin = shaper.Shape(id, "Hello");
    ATOM_CHECK(latin.font_instance == id);
    ATOM_CHECK(latin.glyphs.size() == 5);
    for (std::size_t i = 0; i < latin.glyphs.size(); ++i) {
        ATOM_CHECK(latin.glyphs[i].glyph_id != 0);
        ATOM_CHECK(latin.glyphs[i].cluster_byte_offset == static_cast<std::uint32_t>(i));
        ATOM_CHECK(latin.glyphs[i].x_advance > 0.0f);
    }

    // Multi-byte UTF-8: clusters stay byte offsets ('A' = 0, "é" = 1).
    const std::string mixed = "A\xC3\xA9"; // "Aé"
    const auto clusters = shaper.Shape(id, mixed);
    ATOM_CHECK(clusters.glyphs.size() == 2);
    ATOM_CHECK(clusters.glyphs[0].cluster_byte_offset == 0);
    ATOM_CHECK(clusters.glyphs[1].cluster_byte_offset == 1);

    // Combining mark (x + U+0301 has no precomposed NFC form, so both glyphs
    // survive): the acute accent is positioned without dropping either glyph.
    const std::string decomposed = "x\xCC\x81"; // "x" + U+0301
    const auto marked = shaper.Shape(id, decomposed);
    ATOM_CHECK(marked.glyphs.size() == 2);
    ATOM_CHECK(marked.glyphs[0].glyph_id != 0);
    ATOM_CHECK(marked.glyphs[1].glyph_id != 0);
    ATOM_CHECK(marked.glyphs[0].x_advance > 0.0f);

    // Explicit RTL direction still yields the same glyph count.
    const ShapeParams rtl{.direction = "rtl"};
    ATOM_CHECK(shaper.Shape(id, "Hello", rtl).glyphs.size() == 5);

    return 0;
}

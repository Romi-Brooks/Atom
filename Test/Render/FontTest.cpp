/**
 * @file           : FontTest.cpp
 * @brief          : FreeType-backed Font provider smoke/regression coverage.
 */

#include <Render/Text/Font.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
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
    if (atom::render::Font::CreateFromMemory({}))
        return atom::test::Fail("Font accepted empty input");

    // The pinned HarfBuzz source supplies this test face; Atom does not copy
    // or alter third-party assets. It gives the provider a real OpenType font
    // without making a host-specific system font part of the test contract.
    const auto bytes = ReadBinary(std::filesystem::path{ATOM_SOURCE_ROOT} / "ThirdParty/HarfBuzz/perf/fonts/Roboto-Regular.ttf");
    if (bytes.empty())
        return atom::test::Fail("Pinned font fixture is unavailable");
    const auto font = atom::render::Font::CreateFromMemory(bytes);
    ATOM_CHECK(font);
    ATOM_CHECK(font->IsValid());
    ATOM_CHECK(font->HasGlyph('A'));
    ATOM_CHECK(!font->HasGlyph(0x110000u));

    const auto scale = font->ScaleForPixelHeight(21.0f);
    ATOM_CHECK(scale > 0.0f);
    const auto metrics = font->MetricsForPixelHeight(21.0f);
    ATOM_CHECK(metrics.ascent > 0.0f);
    ATOM_CHECK(metrics.descent <= 0.0f);

    atom::render::RasterizedGlyph glyph{};
    ATOM_CHECK(font->Rasterize('A', scale, glyph));
    ATOM_CHECK(glyph.width > 0 && glyph.height > 0);
    ATOM_CHECK(glyph.rgba.size() == static_cast<std::size_t>(glyph.width) * glyph.height * 4u);
    bool has_coverage = false;
    for (std::size_t index = 3; index < glyph.rgba.size(); index += 4) {
        if (glyph.rgba[index] != 0) {
            has_coverage = true;
            break;
        }
    }
    ATOM_CHECK(has_coverage);
    ATOM_CHECK(font->Advance('V', 'A', scale) > 0.0f);
    return 0;
}

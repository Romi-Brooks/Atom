/**
 * @file           : FreeTypeFontProviderTest.cpp
 * @brief          : W2a contract coverage for the FreeType-backed FontProvider.
 *
 * Exercises the first concrete FontProvider end to end: family registration,
 * instance creation/handout, codepoint -> glyph-id mapping, face metrics, and
 * on-demand rasterization into a format-tagged GlyphRaster.
 */

#include <Render/Text/FreeTypeFontProvider.hpp>
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
    using namespace atom::text;

    FreeTypeFontProvider provider{};

    // Empty family / empty bytes are rejected up front.
    const auto empty = ReadBinary(std::filesystem::path{ATOM_SOURCE_ROOT} / "ThirdParty/HarfBuzz/perf/fonts/Roboto-Regular.ttf");
    if (empty.empty())
        return atom::test::Fail("Pinned font fixture is unavailable");
    ATOM_CHECK(!provider.AddFamily("", empty));
    ATOM_CHECK(!provider.AddFamily("Roboto", {}));

    // An unknown family yields an invalid instance.
    const auto missing = provider.CreateInstance("NoSuchFamily", 21.0f);
    ATOM_CHECK(!missing.IsValid());
    ATOM_CHECK(provider.GetInstance(missing) == nullptr);

    // Register the pinned face and hand out a live instance.
    ATOM_CHECK(provider.AddFamily("Roboto", empty));
    const auto id = provider.CreateInstance("Roboto", 21.0f);
    ATOM_CHECK(id.IsValid());

    const FontInstance* instance = provider.GetInstance(id);
    ATOM_CHECK(instance != nullptr);
    ATOM_CHECK(instance->Id() == id);
    ATOM_CHECK(provider.GetInstance(FontInstanceId{999}) == nullptr); // an id that was never handed out

    // Codepoint -> glyph-id mapping and coverage probe.
    const auto glyph_a = instance->GlyphIndex('A');
    ATOM_CHECK(glyph_a != 0);
    ATOM_CHECK(instance->HasGlyph('A'));
    ATOM_CHECK(!instance->HasGlyph(0x110000u));
    ATOM_CHECK(instance->GlyphIndex(0x110000u) == 0);

    // Face metrics at a pixel height (top-left origin, y-down).
    const auto metrics = instance->Metrics(21.0f);
    ATOM_CHECK(metrics.ascent > 0.0f);
    ATOM_CHECK(metrics.descent <= 0.0f);

    // On-demand rasterization of a real glyph -> non-empty alpha coverage.
    GlyphRaster raster{};
    ATOM_CHECK(instance->Rasterize(glyph_a, 21.0f, RenderMode::Normal, raster));
    ATOM_CHECK(raster.format == RasterFormat::AlphaBitmap);
    ATOM_CHECK(raster.width > 0 && raster.height > 0);
    ATOM_CHECK(raster.pixels.size() == static_cast<std::size_t>(raster.width) * raster.height);
    bool has_coverage = false;
    for (const auto pixel : raster.pixels) {
        if (pixel != 0) {
            has_coverage = true;
            break;
        }
    }
    ATOM_CHECK(has_coverage);

    // An out-of-range glyph id (glyph 0 is the valid .notdef) rasterizes to failure.
    GlyphRaster missing_glyph{};
    ATOM_CHECK(!instance->Rasterize(0xFFFFFFFFu, 21.0f, RenderMode::Normal, missing_glyph));

    // No fallback chain in W2a.
    ATOM_CHECK(!provider.FallbackFor(id, 'A').IsValid());

    return 0;
}

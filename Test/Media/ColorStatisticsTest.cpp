#include <Media/Image/Analysis/ColorStatistics.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cstdint>
#include <vector>

namespace {

auto MakeImage(const uint32_t width, const uint32_t height, const std::vector<uint8_t>& rgba) -> atom::image::DecodedImage {
    atom::image::DecodedImage image{};
    image.width = width;
    image.height = height;
    image.rgba = rgba;
    return image;
}

} // namespace

auto main() -> int {
    using namespace atom::image;

    ATOM_CHECK(!CalculateColorStatistics(DecodedImage{}).has_value());

    // 2x1: black then white, opaque.
    const auto two_tone = MakeImage(2, 1, {0, 0, 0, 255, 255, 255, 255, 255});
    const auto stats = CalculateColorStatistics(two_tone);
    ATOM_CHECK(stats.has_value());
    ATOM_CHECK(stats->minimum.r == 0 && stats->minimum.g == 0 && stats->minimum.b == 0);
    ATOM_CHECK(stats->maximum.r == 255 && stats->maximum.g == 255 && stats->maximum.b == 255);
    ATOM_CHECK(stats->median.r == 0 || stats->median.r == 255);

    // Transparent pixels are skipped.
    const auto mixed = MakeImage(2, 1, {0, 0, 0, 255, 255, 255, 255, 0});
    const auto mixed_stats = CalculateColorStatistics(mixed);
    ATOM_CHECK(mixed_stats.has_value());
    ATOM_CHECK(mixed_stats->minimum.r == 0 && mixed_stats->maximum.r == 0);

    // All transparent -> no samples.
    const auto clear = MakeImage(2, 1, {255, 255, 255, 0, 255, 255, 255, 0});
    ATOM_CHECK(!CalculateColorStatistics(clear).has_value());

    // 2x2 mid gray average.
    const auto gray = MakeImage(2, 2, {
        10, 10, 10, 255,
        30, 30, 30, 255,
        50, 50, 50, 255,
        70, 70, 70, 255,
    });
    const auto gray_stats = CalculateColorStatistics(gray);
    ATOM_CHECK(gray_stats.has_value());
    ATOM_CHECK(gray_stats->average.r == 40);
    ATOM_CHECK(gray_stats->minimum.r == 10);
    ATOM_CHECK(gray_stats->maximum.r == 70);
    return 0;
}

/**
 * @file           : ColorStatistics.cpp
 * @brief          : Luminance-ordered image color statistic implementation.
 */

#include "ColorStatistics.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace atom::image {
namespace {

[[nodiscard]] constexpr auto RelativeLuminance(const color::Color color) -> float {
    return 0.2126f * static_cast<float>(color.r) + 0.7152f * static_cast<float>(color.g) +
           0.0722f * static_cast<float>(color.b);
}

} // namespace

auto CalculateColorStatistics(const DecodedImage& image, ColorStatisticsOptions options)
    -> std::optional<ColorStatistics> {
    if (!image.IsValid())
        return std::nullopt;

    const auto stride = std::max(1u, options.sample_stride);
    std::vector<color::Color> samples;
    samples.reserve(image.rgba.size() / (4u * stride));
    uint64_t sum_red = 0;
    uint64_t sum_green = 0;
    uint64_t sum_blue = 0;
    uint64_t sum_alpha = 0;

    for (std::size_t pixel = 0; pixel < image.rgba.size() / 4u; pixel += stride) {
        const auto offset = pixel * 4u;
        const auto alpha = image.rgba[offset + 3];
        if (alpha < options.minimum_alpha)
            continue;
        const color::Color sample{image.rgba[offset], image.rgba[offset + 1], image.rgba[offset + 2], alpha};
        samples.push_back(sample);
        sum_red += sample.r;
        sum_green += sample.g;
        sum_blue += sample.b;
        sum_alpha += sample.a;
    }
    if (samples.empty())
        return std::nullopt;

    std::ranges::sort(samples, {}, RelativeLuminance);
    const auto count = samples.size();
    return ColorStatistics{samples.front(), samples[count / 2u], samples.back(),
                           {static_cast<uint8_t>(sum_red / count), static_cast<uint8_t>(sum_green / count),
                            static_cast<uint8_t>(sum_blue / count), static_cast<uint8_t>(sum_alpha / count)}};
}

} // namespace atom::image

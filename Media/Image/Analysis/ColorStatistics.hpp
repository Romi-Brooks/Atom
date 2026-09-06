/**
 * @file           : ColorStatistics.hpp
 * @brief          : Luminance-ordered color statistics for decoded RGBA images.
 */

#ifndef ATOM_MEDIA_IMAGE_ANALYSIS_COLOR_STATISTICS_HPP
#define ATOM_MEDIA_IMAGE_ANALYSIS_COLOR_STATISTICS_HPP

#include <optional>

#include <Color/Color.hpp>
#include <Media/Image/ImageDecoder.hpp>

namespace atom::image {

struct ColorStatistics {
        color::Color minimum{};
        color::Color median{};
        color::Color maximum{};
        color::Color average{};
};

struct ColorStatisticsOptions {
        uint8_t minimum_alpha = 32;
        uint32_t sample_stride = 4;
};

// Samples valid pixels, orders their colors by Rec. 709 relative luminance,
// and returns the darkest, median, brightest, and arithmetic-average colors.
// These are image facts only; palette/theme/filter policy belongs to callers.
[[nodiscard]] auto CalculateColorStatistics(const DecodedImage& image,
                                            ColorStatisticsOptions options = {}) -> std::optional<ColorStatistics>;

} // namespace atom::image

#endif // ATOM_MEDIA_IMAGE_ANALYSIS_COLOR_STATISTICS_HPP

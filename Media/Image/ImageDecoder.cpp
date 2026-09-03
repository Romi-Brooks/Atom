/**
  * @file           : ImageDecoder.cpp
  * @author         : Romi Brooks
  * @brief          : stb_image-backed RGBA image decoding implementation.
  * @attention      : The vendored stb implementation macro is defined only here.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "ImageDecoder.hpp"

#include <limits>
#include <memory>

#include <Log/LogSystem.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace atom::image {
namespace {

auto WrapResult(int width, int height, unsigned char* pixels, bool flip) -> DecodedImage {
    DecodedImage result{};
    if (!pixels || width <= 0 || height <= 0)
        return result;
    const auto max = std::numeric_limits<std::size_t>::max();
    if (static_cast<std::size_t>(height) > max / static_cast<std::size_t>(width))
        return result;
    const auto pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixelCount > max / 4u)
        return result;
    result.width = static_cast<uint32_t>(width);
    result.height = static_cast<uint32_t>(height);
    const auto count = pixelCount * 4u;
    result.rgba.assign(pixels, pixels + count);
    if (flip) {
        // stb_image returns rows top-down; flip to bottom-up by row swap.
        const auto row = static_cast<std::size_t>(width) * 4u;
        for (std::size_t y = 0; y < result.height / 2; ++y) {
            std::swap_ranges(result.rgba.begin() + y * row, result.rgba.begin() + (y + 1) * row,
                             result.rgba.begin() + (result.height - 1 - y) * row);
        }
    }
    return result;
}

} // namespace

auto DecodeImageMemory(std::span<const std::byte> data, const bool flip_vertically) -> DecodedImage {
    if (data.empty() || data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        LOG_WARNING(atom::image::LogChannel::DECODER, "Image memory input is empty or exceeds stb_image limits");
        return {};
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    auto* raw_pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data.data()),
                                             static_cast<int>(data.size()), &width, &height, &channels, STBI_rgb_alpha);
    if (!raw_pixels) {
        const char* reason = stbi_failure_reason();
        LOG_WARNING(atom::image::LogChannel::DECODER,
                    "Image memory decode failed: " + std::string{reason ? reason : "unknown stb_image error"});
        return {};
    }
    const auto deleter = [](stbi_uc* pixels) { stbi_image_free(pixels); };
    const std::unique_ptr<stbi_uc, decltype(deleter)> pixels{raw_pixels, deleter};
    DecodedImage result = WrapResult(width, height, pixels.get(), flip_vertically);
    if (result.IsValid())
        LOG_DEBUG(atom::image::LogChannel::DECODER, "Decoded image from memory (" + std::to_string(result.width) + "x" +
                                                        std::to_string(result.height) + ")");
    return result;
}

auto DecodeImageFile(const std::string& path, const bool flip_vertically) -> DecodedImage {
    if (path.empty()) {
        LOG_WARNING(atom::image::LogChannel::DECODER, "Image file decode rejected an empty path");
        return {};
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    auto* raw_pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!raw_pixels) {
        const char* reason = stbi_failure_reason();
        LOG_WARNING(atom::image::LogChannel::DECODER, "Image file decode failed ('" + path + "'): " +
                                                          std::string{reason ? reason : "unknown stb_image error"});
        return {};
    }
    const auto deleter = [](stbi_uc* pixels) { stbi_image_free(pixels); };
    const std::unique_ptr<stbi_uc, decltype(deleter)> pixels{raw_pixels, deleter};
    DecodedImage result = WrapResult(width, height, pixels.get(), flip_vertically);
    if (result.IsValid())
        LOG_DEBUG(atom::image::LogChannel::DECODER, "Decoded image file '" + path + "' (" +
                                                        std::to_string(result.width) + "x" +
                                                        std::to_string(result.height) + ")");
    return result;
}

} // namespace atom::image

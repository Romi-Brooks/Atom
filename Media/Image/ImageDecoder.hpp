/**
  * @file           : ImageDecoder.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral RGBA image decoding API.
  * @attention      : Decoding is CPU-only; GPU texture creation is handled by render modules.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_MEDIA_IMAGE_IMAGEDECODER_HPP
#define ATOM_MEDIA_IMAGE_IMAGEDECODER_HPP

#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace atom::image {

// CPU-side decoded RGBA image. Kept deliberately separate from any GPU
// resource: decoding is pure CPU work and may happen on any thread/context,
// while uploading into a Renderer2D texture is a separate later step.
struct DecodedImage {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> rgba{}; // width * height * 4, straight alpha

        [[nodiscard]] auto IsValid() const -> bool {
            if (width == 0 || height == 0)
                return false;
            const auto max = std::numeric_limits<std::size_t>::max();
            if (static_cast<std::size_t>(height) > max / static_cast<std::size_t>(width))
                return false;
            const auto pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            return pixels <= max / 4u && rgba.size() == pixels * 4u;
        }
        void Reset() {
            width = 0;
            height = 0;
            rgba.clear();
        }
};

// Decodes formats supported by stb_image (PNG, JPEG, BMP, GIF, TGA, PSD,
// HDR, PIC and PNM; WebP is not supported by stb_image)
// into tightly packed RGBA8, top-left origin, row-major (matches the texture
// upload layout used by Renderer2D / SDL_GPU). Returns an invalid image when
// the data cannot be decoded.
[[nodiscard]] auto DecodeImageMemory(std::span<const std::byte> data, bool flip_vertically = false) -> DecodedImage;
[[nodiscard]] auto DecodeImageFile(const std::string& path, bool flip_vertically = false) -> DecodedImage;

} // namespace atom::image

#endif // ATOM_MEDIA_IMAGE_IMAGEDECODER_HPP

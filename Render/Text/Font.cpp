/**
  * @file           : Font.cpp
  * @author         : Romi Brooks
  * @brief          : Trusted-font glyph rasterization implementation.
  * @attention      : GPU upload is intentionally owned by the render backend, not this module.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Font.hpp"

#include <cmath>
#include <cstring>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace atom::render {

struct Font::Impl {
        std::vector<uint8_t> bytes{};
        stbtt_fontinfo info{};
        int ascent = 0;
        int descent = 0;
        int line_gap = 0;
        bool valid = false;
};

Font::~Font() = default;
Font::Font(Font&&) noexcept = default;
auto Font::operator=(Font&&) noexcept -> Font& = default;

auto Font::CreateFromMemory(const std::span<const std::byte> bytes) -> std::unique_ptr<Font> {
    if (bytes.empty())
        return nullptr;
    auto font = std::make_unique<Font>();
    font->impl_ = std::make_unique<Impl>();
    font->impl_->bytes.resize(bytes.size());
    std::memcpy(font->impl_->bytes.data(), bytes.data(), bytes.size());
    const int offset = stbtt_GetFontOffsetForIndex(font->impl_->bytes.data(), 0);
    font->impl_->valid = offset >= 0 && stbtt_InitFont(&font->impl_->info, font->impl_->bytes.data(), offset) != 0;
    if (font->impl_->valid)
        stbtt_GetFontVMetrics(&font->impl_->info, &font->impl_->ascent, &font->impl_->descent,
                              &font->impl_->line_gap);
    if (!font->impl_->valid)
        return nullptr;
    return font;
}

auto Font::IsValid() const -> bool { return impl_ && impl_->valid; }

auto Font::MetricsForPixelHeight(const float pixel_height) const -> FontMetricsPx {
    if (!IsValid() || !std::isfinite(pixel_height) || pixel_height <= 0.0f)
        return {};
    const float scale = stbtt_ScaleForPixelHeight(&impl_->info, pixel_height);
    return {static_cast<float>(impl_->ascent) * scale, static_cast<float>(impl_->descent) * scale,
            static_cast<float>(impl_->line_gap) * scale};
}

auto Font::HasGlyph(const uint32_t codepoint) const -> bool {
    return IsValid() && codepoint <= 0x10ffffu &&
           stbtt_FindGlyphIndex(&impl_->info, static_cast<int>(codepoint)) != 0;
}

auto Font::ScaleForPixelHeight(const float pixel_height) const -> float {
    if (!IsValid() || !std::isfinite(pixel_height) || pixel_height <= 0.0f)
        return 0.0f;
    return stbtt_ScaleForPixelHeight(&impl_->info, pixel_height);
}

auto Font::Rasterize(const uint32_t codepoint, const float scale, RasterizedGlyph& out) const -> bool {
    out = {};
    if (!HasGlyph(codepoint) || !std::isfinite(scale) || scale <= 0.0f)
        return false;
    int width = 0;
    int height = 0;
    int xoff = 0;
    int yoff = 0;
    auto* mono = stbtt_GetCodepointBitmap(&impl_->info, scale, scale, static_cast<int>(codepoint), &width, &height,
                                          &xoff, &yoff);
    if (!mono || width <= 0 || height <= 0) {
        if (mono)
            stbtt_FreeBitmap(mono, nullptr);
        return false;
    }
    const auto deleter = [](unsigned char* pixels) { stbtt_FreeBitmap(pixels, nullptr); };
    const std::unique_ptr<unsigned char, decltype(deleter)> bitmap{mono, deleter};
    out.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto value = bitmap.get()[y * width + x];
            auto* pixel = out.rgba.data() +
                          (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                           static_cast<std::size_t>(x)) * 4u;
            pixel[0] = 255;
            pixel[1] = 255;
            pixel[2] = 255;
            pixel[3] = value;
        }
    }
    int advance_width = 0;
    int left_side_bearing = 0;
    stbtt_GetCodepointHMetrics(&impl_->info, static_cast<int>(codepoint), &advance_width, &left_side_bearing);
    out.width = static_cast<uint32_t>(width);
    out.height = static_cast<uint32_t>(height);
    out.offset_x = xoff;
    out.offset_y = yoff;
    out.advance = static_cast<float>(advance_width) * scale;
    return true;
}

auto Font::Advance(const uint32_t codepoint, const uint32_t previous, const float scale) const -> float {
    if (!IsValid() || codepoint > 0x10ffffu || previous > 0x10ffffu || !std::isfinite(scale) || scale <= 0.0f)
        return 0.0f;
    int advance_width = 0;
    int left_side_bearing = 0;
    stbtt_GetCodepointHMetrics(&impl_->info, static_cast<int>(codepoint), &advance_width, &left_side_bearing);
    float advance = static_cast<float>(advance_width) * scale;
    if (previous != 0) {
        const int kern = stbtt_GetCodepointKernAdvance(&impl_->info, static_cast<int>(previous),
                                                       static_cast<int>(codepoint));
        advance += static_cast<float>(kern) * scale;
    }
    return advance;
}

} // namespace atom::render

/**
  * @file           : Font.cpp
  * @author         : Romi Brooks
  * @brief          : Trusted-font glyph rasterization implementation.
  * @attention      : GPU upload is intentionally owned by the render backend, not this module.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Font.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace atom::render {

struct Font::Impl {
        std::vector<uint8_t> bytes{};
        FT_Library library = nullptr;
        FT_Face face = nullptr;
        mutable std::mutex mutex{};
        bool valid = false;

        ~Impl() {
            if (face)
                FT_Done_Face(face);
            if (library)
                FT_Done_FreeType(library);
        }
};

namespace {

[[nodiscard]] auto PixelHeight(const float scale) -> FT_UInt {
    if (!std::isfinite(scale) || scale <= 0.0f)
        return 0;
    return static_cast<FT_UInt>(std::max(1.0f, std::round(scale)));
}

[[nodiscard]] auto BitmapRow(const FT_Bitmap& bitmap, const uint32_t row) -> const uint8_t* {
    const auto stride = static_cast<std::ptrdiff_t>(std::abs(bitmap.pitch));
    if (bitmap.pitch >= 0)
        return bitmap.buffer + static_cast<std::ptrdiff_t>(row) * stride;
    return bitmap.buffer + static_cast<std::ptrdiff_t>(bitmap.rows - 1u - row) * stride;
}

} // namespace

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
    font->impl_->valid =
        FT_Init_FreeType(&font->impl_->library) == 0 &&
        FT_New_Memory_Face(font->impl_->library, font->impl_->bytes.data(),
                           static_cast<FT_Long>(font->impl_->bytes.size()), 0, &font->impl_->face) == 0;
    if (!font->impl_->valid)
        return nullptr;
    return font;
}

auto Font::IsValid() const -> bool { return impl_ && impl_->valid; }

auto Font::MetricsForPixelHeight(const float pixel_height) const -> FontMetricsPx {
    if (!IsValid() || !std::isfinite(pixel_height) || pixel_height <= 0.0f)
        return {};
    std::scoped_lock lock{impl_->mutex};
    const FT_UInt pixel_size = PixelHeight(pixel_height);
    if (pixel_size == 0 || FT_Set_Pixel_Sizes(impl_->face, 0, pixel_size) != 0 || !impl_->face->size)
        return {};
    const auto& metrics = impl_->face->size->metrics;
    const float ascent = static_cast<float>(metrics.ascender) / 64.0f;
    const float descent = static_cast<float>(metrics.descender) / 64.0f;
    const float line_height = static_cast<float>(metrics.height) / 64.0f;
    return {ascent, descent, std::max(line_height - (ascent - descent), 0.0f)};
}

auto Font::HasGlyph(const uint32_t codepoint) const -> bool {
    if (!IsValid() || codepoint > 0x10ffffu)
        return false;
    std::scoped_lock lock{impl_->mutex};
    return FT_Get_Char_Index(impl_->face, codepoint) != 0;
}

auto Font::ScaleForPixelHeight(const float pixel_height) const -> float {
    if (!IsValid() || !std::isfinite(pixel_height) || pixel_height <= 0.0f)
        return 0.0f;
    // Renderer2D treats this value as an opaque raster instance key.  In the
    // FreeType provider it is the requested pixel height, rather than a font
    // unit scale as it was in the old stb implementation.
    return pixel_height;
}

auto Font::Rasterize(const uint32_t codepoint, const float scale, RasterizedGlyph& out) const -> bool {
    out = {};
    // Codepoint zero deliberately rasterizes the font's .notdef glyph. The
    // renderer uses it as a visible fallback when a requested glyph is absent.
    if (!IsValid() || (codepoint != 0 && !HasGlyph(codepoint)) || !std::isfinite(scale) || scale <= 0.0f)
        return false;
    std::scoped_lock lock{impl_->mutex};
    const FT_UInt height = PixelHeight(scale);
    if (height == 0 || FT_Set_Pixel_Sizes(impl_->face, 0, height) != 0)
        return false;
    const FT_UInt glyph_index = codepoint == 0 ? 0u : FT_Get_Char_Index(impl_->face, codepoint);
    if (FT_Load_Glyph(impl_->face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT) != 0 ||
        FT_Render_Glyph(impl_->face->glyph, FT_RENDER_MODE_NORMAL) != 0)
        return false;
    const auto& bitmap = impl_->face->glyph->bitmap;
    if (!bitmap.buffer || bitmap.width == 0 || bitmap.rows == 0)
        return false;
    out.rgba.resize(static_cast<std::size_t>(bitmap.width) * bitmap.rows * 4u);
    for (uint32_t y = 0; y < bitmap.rows; ++y) {
        const auto* row = BitmapRow(bitmap, y);
        for (uint32_t x = 0; x < bitmap.width; ++x) {
            uint8_t red = 255;
            uint8_t green = 255;
            uint8_t blue = 255;
            uint8_t alpha = 0;
            if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                alpha = row[x];
            } else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                alpha = (row[x / 8u] & (0x80u >> (x % 8u))) != 0 ? 255 : 0;
            } else if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                const auto* pixel = row + static_cast<std::size_t>(x) * 4u;
                blue = pixel[0];
                green = pixel[1];
                red = pixel[2];
                alpha = pixel[3];
            } else {
                return false;
            }
            auto* pixel = out.rgba.data() +
                          (static_cast<std::size_t>(y) * bitmap.width + x) * 4u;
            pixel[0] = red;
            pixel[1] = green;
            pixel[2] = blue;
            pixel[3] = alpha;
        }
    }
    out.width = bitmap.width;
    out.height = bitmap.rows;
    out.offset_x = impl_->face->glyph->bitmap_left;
    out.offset_y = -impl_->face->glyph->bitmap_top;
    return true;
}

auto Font::Advance(const uint32_t codepoint, const uint32_t previous, const float scale) const -> float {
    if (!IsValid() || codepoint > 0x10ffffu || previous > 0x10ffffu || !std::isfinite(scale) || scale <= 0.0f)
        return 0.0f;
    std::scoped_lock lock{impl_->mutex};
    const FT_UInt height = PixelHeight(scale);
    if (height == 0 || FT_Set_Pixel_Sizes(impl_->face, 0, height) != 0)
        return 0.0f;
    const FT_UInt glyph_index = FT_Get_Char_Index(impl_->face, codepoint);
    if (FT_Load_Glyph(impl_->face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT) != 0)
        return 0.0f;
    float advance = static_cast<float>(impl_->face->glyph->advance.x) / 64.0f;
    const FT_UInt previous_index = FT_Get_Char_Index(impl_->face, previous);
    if (previous != 0 && previous_index != 0 && glyph_index != 0 && FT_HAS_KERNING(impl_->face)) {
        FT_Vector kerning{};
        if (FT_Get_Kerning(impl_->face, previous_index, glyph_index, FT_KERNING_DEFAULT, &kerning) == 0)
            advance += static_cast<float>(kerning.x) / 64.0f;
    }
    return advance;
}

} // namespace atom::render

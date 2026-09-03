/**
  * @file           : Font.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral trusted-font rasterization and glyph atlas data.
  * @attention      : stb_truetype is a lightweight provider; shaping and fallback remain future work.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_RENDER_TEXT_FONT_HPP
#define ATOM_RENDER_TEXT_FONT_HPP

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace atom::render {

struct FontMetricsPx {
        float ascent = 0.0f;
        float descent = 0.0f;
        float line_gap = 0.0f;
};

// CPU-only single-face font provider. Renderer2D owns GPU atlas policy; this
// type deliberately knows nothing about SDL_GPU, Vulkan or a render target.
struct RasterizedGlyph {
        std::vector<uint8_t> rgba;
        uint32_t width = 0;
        uint32_t height = 0;
        int offset_x = 0;
        int offset_y = 0;
        float advance = 0.0f;
};

// Lightweight stb_truetype provider. The API is provider-shaped so a future
// FreeType + HarfBuzz implementation can replace it without changing the
// render backend or Renderer2D public contract.
class Font {
    public:
        Font() = default;
        ~Font();
        Font(const Font&) = delete;
        auto operator=(const Font&) -> Font& = delete;
        Font(Font&&) noexcept;
        auto operator=(Font&&) noexcept -> Font&;

        [[nodiscard]] static auto CreateFromMemory(std::span<const std::byte> bytes) -> std::unique_ptr<Font>;
        [[nodiscard]] auto IsValid() const -> bool;
        [[nodiscard]] auto MetricsForPixelHeight(float pixel_height) const -> FontMetricsPx;
        [[nodiscard]] auto HasGlyph(uint32_t codepoint) const -> bool;
        [[nodiscard]] auto ScaleForPixelHeight(float pixel_height) const -> float;
        auto Rasterize(uint32_t codepoint, float scale, RasterizedGlyph& out) const -> bool;
        [[nodiscard]] auto Advance(uint32_t codepoint, uint32_t previous, float scale) const -> float;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_{};
};

} // namespace atom::render

#endif // ATOM_RENDER_TEXT_FONT_HPP

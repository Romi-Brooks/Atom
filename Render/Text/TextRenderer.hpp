/**
 * @file           : TextRenderer.hpp
 * @brief          : Backend-neutral text-render contract.
 *
 * Consumes an immutable TextLayout and emits glyph quads. Backend-neutral: it
 * never touches SDL_GPU. The quads carry enough info for the render backend to
 * pick a material/shader by `format` (alpha blend for bitmaps, px-range
 * distance field for SDF/MSDF) and bind the right atlas page. The same layout
 * can therefore feed a 2D HUD and a 3D world billboard.
 *
 * W1 (contract freeze): the Renderer2D adapter lands in W3; nothing here
 * depends on Renderer2D internals.
**/

#ifndef ATOM_RENDER_TEXT_TEXT_RENDERER_HPP
#define ATOM_RENDER_TEXT_TEXT_RENDERER_HPP

#include <cstdint>
#include <vector>

#include <Render/Text/TextLayout.hpp>
#include <Render/Text/TextTypes.hpp>

namespace atom::text {

// One drawable glyph quad, positioned in layout space (top-left origin).
struct GlyphQuad {
        std::uint32_t page = 0; // atlas page (resolved to texture by the backend)
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        RasterFormat format = RasterFormat::AlphaBitmap;
};

// Consumes a layout and emits glyph quads. The backend converts quads into its
// own draw packets, selecting the material by `format`.
class TextRenderer {
    public:
        virtual ~TextRenderer() = default;
        TextRenderer(const TextRenderer&) = delete;
        TextRenderer& operator=(const TextRenderer&) = delete;

        // Emits quads for every positioned glyph in `layout`, translated by
        // (origin_x, origin_y). The atlas page + UV come from the GlyphAtlas the
        // renderer was configured with.
        virtual auto Emit(const TextLayout& layout, float origin_x, float origin_y,
                          std::vector<GlyphQuad>& out) -> void = 0;

    protected:
        TextRenderer() = default;
};

} // namespace atom::text

#endif // ATOM_RENDER_TEXT_TEXT_RENDERER_HPP

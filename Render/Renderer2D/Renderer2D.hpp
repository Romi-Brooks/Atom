/**
  * @file           : Renderer2D.hpp
  * @author         : Romi Brooks
  * @brief          : Public batched 2D drawing API over a render device.
  * @attention      : The API is backend-neutral and does not expose SDL_GPU types.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_RENDER_RENDERER2D_HPP
#define ATOM_RENDER_RENDERER2D_HPP

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Algorithm/Geometry/Primitives.hpp>
#include <Algorithm/Geometry/Rect.hpp>
#include <Backend/Contracts/Render/IRender2DContext.hpp>

namespace atom::render {

class Font;
class IRenderDevice;

// Measured extent of a text run, in the same logical pixel space DrawText uses.
// `height` is the laid-out line box height (line count * line height); `width`
// is the widest line's advance. Both are produced by the exact same layout
// state machine DrawText runs, so measurements never drift from what is drawn.
struct TextSize {
        float width = 0.0f;
        float height = 0.0f;
};

// A single glyph's laid-out pen origin, relative to the run's top-left
// (0, 0). `x` is the position after kerning; `y` is its line's top.
struct TextGlyphRun {
        uint32_t codepoint = 0;
        float x = 0.0f;
        float y = 0.0f;
};

// Output of the shared text-layout pass: glyph origins (relative to origin)
// plus the bounding size. Cached per (font, text, size, wrap) so a static or
// scrolling run is laid out once, not once per frame.
struct TextLayoutResult {
        float width = 0.0f;
        float height = 0.0f;
        float line_height = 0.0f;
        std::vector<TextGlyphRun> glyphs{};
};

// Cache key for TextLayoutResult. `text` is stored by value so the key owns
// its string; the font is identified by pointer (fonts are long-lived).
struct TextLayoutKey {
        const Font* font = nullptr;
        float size_px = 0.0f;
        float max_width = 0.0f;
        std::string text;
        auto operator==(const TextLayoutKey&) const -> bool = default;
};

struct TextLayoutKeyHash {
        auto operator()(const TextLayoutKey& key) const noexcept -> std::size_t;
};

// High-level batched 2D renderer (Render/ roadmap: Renderer2D). Coordinates
// are logical "world" pixels with a top-left origin and y pointing down; the
// camera (origin + zoom) maps world space onto the viewport. Drawing is
// recorded between BeginFrame()/EndFrame() and flushed in batched GPU
// submissions through the backend's IRender2DContext, so this class never
// touches SDL_GPU / Vulkan types.
class Renderer2D {
    public:
        // Parameters for an optional post-process pass. The effect itself is a
        // fullscreen algorithm; `has_region` optionally confines it to a
        // rounded-rectangle sub-region (region + corner_radius + feather). With
        // has_region == false the effect runs over the whole frame with no
        // region-mask cost, so it is a generic screen-space filter usable by
        // any renderer client.
        struct PostProcessParams {
                PostProcess2DEffect effect = PostProcess2DEffect::None;
                bool has_region = false;
                algo::Rect region{};
                float corner_radius = 0.0f;
                float feather = 0.0f;
                float amount = 0.0f;
                float scanline = 0.0f;
                float noise = 0.0f;
                float progress = 0.0f;
                float intensity = 0.0f;
                float time = 0.0f;
                float direction = 1.0f;
        };

        // A GPU texture owned by this renderer (RGBA8). Pointers stay valid
        // until DestroyTexture() or Shutdown(). Uploads are deferred to the
        // next EndFrame(), so Create/Update may be called outside a frame.
        class Texture {
            public:
                [[nodiscard]] auto GetWidth() const -> uint32_t {
                    return width_;
                }
                [[nodiscard]] auto GetHeight() const -> uint32_t {
                    return height_;
                }

            private:
                friend class Renderer2D;
                Renderer2D* owner_ = nullptr;
                render::Texture2D handle_ = render::invalid_texture_2d;
                uint32_t width_ = 0;
                uint32_t height_ = 0;
        };

        // Liveness of this renderer's texture pool.
        //
        // Shutdown() destroys every Renderer2D::Texture, but handles that refer
        // to those textures can outlive it: Render/Resources/TextureCache.hpp
        // hands out shareable handles, and the code that keeps them (a Screen,
        // a resource pack, a cache) is not required to drop them before the
        // renderer is torn down. A stale Texture* must therefore never be
        // dereferenced, not even to ask whether it is still owned.
        //
        // Every texture handle keeps this state alive and consults `alive`
        // before it exposes or reclaims its texture. Shutdown() clears the flag
        // and Initialize() installs a fresh state, so a handle from an earlier
        // generation degrades to an inert value instead of a dangling pointer.
        struct TexturePoolState final {
                bool alive = true;
        };
        [[nodiscard]] auto GetTexturePoolState() const -> const std::shared_ptr<TexturePoolState>& {
            return texture_pool_state_;
        }

        Renderer2D();
        ~Renderer2D();
        Renderer2D(const Renderer2D&) = delete;
        auto operator=(const Renderer2D&) -> Renderer2D& = delete;
        Renderer2D(Renderer2D&&) = delete;
        auto operator=(Renderer2D&&) -> Renderer2D& = delete;

        // Resolves the backend 2D context and prepares the built-in pipeline
        // from the compiled shader output directory (ATOM_SHADER_OUTPUT_DIR).
        auto Initialize(IRenderDevice& device, const std::filesystem::path& shader_root) -> bool;
        [[nodiscard]] auto IsInitialized() const -> bool;
        auto Shutdown() -> void;

        // Starts recording a frame; zoom must be > 0 (clamped otherwise).
        auto BeginFrame(float origin_x = 0.0f, float origin_y = 0.0f, float zoom = 1.0f) -> bool;
        // Flushes recorded drawing (uploads + batched render passes). Call
        // while the owning device's frame is active, before its EndFrame().
        auto EndFrame() -> bool;
        [[nodiscard]] auto IsInFrame() const -> bool;

        auto SetPostProcess(const PostProcessParams& params) -> void;

        // Resources:
        // Creates an RGBA8 texture; rgba (w*h*4 bytes) may be null. The upload
        // happens at the next EndFrame(). Returns nullptr on failure.
        //
        // The numeric contract (Backend/Contracts/Numeric/NumericContract.hpp)
        // applies: width and height must each be in [1, max_texture_dimension]
        // (16384) and their RGBA size must be representable, otherwise the call
        // logs an error and returns nullptr.
        auto CreateTexture(uint32_t width, uint32_t height, const void* rgba = nullptr) -> Texture*;
        auto UpdateTexture(Texture& texture, const void* rgba) -> void;
        auto DestroyTexture(Texture& texture) -> void;

        // Deferred GPU reclaim (ARCH-107 / D4). Callers that drop the last
        // reference to a texture while a frame may be active enqueue it here;
        // the actual DestroyTexture2D runs in FlushDeferredTextureDestroys(),
        // which must be called outside an active frame (typically once per
        // frame, before BeginFrame). Shutdown also drains the queue.
        auto EnqueueDeferredTextureDestroy(Texture* texture) -> void;
        auto FlushDeferredTextureDestroys() -> void;

        // Creates a font from TTF/TTC bytes (kept alive by the renderer until
        // DestroyFont() / Shutdown()).
        auto LoadFontFromMemory(std::span<const std::byte> font_data) -> Font*;
        auto DestroyFont(Font* font) -> void;

        [[nodiscard]] auto GetWhiteTexture() const -> Texture*;

        // Drawing (only between BeginFrame() and EndFrame()):
        /**
         * @brief Draws a texture into a geometry/world rectangle.
         * @param texture Renderer-owned texture to draw.
         * @param dst World-space destination rectangle. Renderer2D converts it
         *        internally from algo::Rect to its private render::Rect.
         * @param tint Color multiplied with the texture.
         * @param source Optional texel rectangle in the texture. It is also
         *        converted internally; a dedicated TextureRegion type will
         *        replace this algo::Rect parameter in a future API revision.
         */
        auto DrawTexture(const Texture& texture, const algo::Rect& dst, const Color& tint = Color::White(),
                         const algo::Rect* source = nullptr) -> void;

        /**
         * @brief Draws a filled geometry/world rectangle.
         * @param rect World-space rectangle. Renderer2D converts it internally
         *        from algo::Rect to its private render::Rect before batching.
         * @param color Fill color.
         */
        auto DrawRect(const algo::Rect& rect, const Color& color) -> void;

        /**
         * @brief Draws the outline of a geometry/world rectangle.
         * @param rect World-space rectangle. Renderer2D splits it into four
         *        geometry rectangles, each converted internally before draw.
         * @param color Outline color.
         * @param thickness Outline thickness in world units.
         */
        auto DrawRectOutline(const algo::Rect& rect, const Color& color, float thickness = 1.0f) -> void;

        /**
         * @brief Draws a geometric circle.
         * @param circle Geometry circle. Renderer2D reads its center and radius
         *        internally, then records its private render-circle operation.
         * @param color Fill color.
         * @param segments Tessellation quality; it is a rendering setting and
         *        therefore is not stored in algo::Circle2.
         */
        auto DrawCircle(const algo::Circle2& circle, const Color& color, uint32_t segments = 0) -> void;
        auto DrawCircle(float center_x, float center_y, float radius, const Color& color, uint32_t segments = 0)
            -> void;
        auto DrawLine(float x0, float y0, float x1, float y1, const Color& color, float thickness = 1.0f) -> void;

        // Text is drawn with its first line's top at (x, y). size_px must be
        // within (0, 512]; max_width == 0 disables line wrapping.
        auto DrawText(const Font& font, std::string_view text, float x, float y, const Color& color, float size_px,
                      float max_width = 0.0f) -> void;

        // Measures a text run without recording any draw. Uses the identical
        // layout rules as DrawText (same wrap/word-break state machine), so the
        // returned size matches what DrawText would paint. max_width == 0
        // disables wrapping and returns the single-line advance, which
        // scrolling and truncation rely on. Callable outside a frame.
        [[nodiscard]] auto MeasureText(const Font& font, std::string_view text, float size_px,
                                       float max_width = 0.0f) const -> TextSize;

        /**
         * @brief Pushes a nested world-space scissor clip.
         * @param rect Geometry/world rectangle. Renderer2D converts it
         *        internally from algo::Rect to its private render::Rect before
         *        intersecting it with the parent clip.
         * @note Higher PushLayer values draw later (on top).
         */
        auto PushClip(const algo::Rect& rect) -> void;
        auto PopClip() -> void;
        auto PushLayer(int32_t layer) -> void;
        auto PopLayer() -> void;

    private:
        enum class OpKind : uint8_t { Quad, Circle, Text };

        struct DrawOp {
                OpKind kind = OpKind::Quad;
                int32_t layer = 0;
                Rect clip{};
                bool has_clip = false;
                Color color = Color::White();

                // Quad (axis-aligned bounds) or line (custom corners).
                const Texture* texture = nullptr; // null -> white pixel texture
                render::Sampler2D sampler = render::invalid_sampler_2d;
                Rect dst{};
                Rect source{};
                bool has_source = false;
                bool custom_quad = false;
                std::array<std::array<float, 2>, 4> corners{};

                // Circle.
                float center_x = 0.0f;
                float center_y = 0.0f;
                float radius = 0.0f;
                uint32_t segments = 0;

                // Text.
                const Font* font = nullptr;
                float size_px = 0.0f;
                float max_width = 0.0f;
                std::string text{};
        };

        struct GlyphPage {
                Texture* texture = nullptr;
                uint32_t size = 0;         // square page edge in pixels
                std::vector<uint8_t> rgba; // size * size * 4
                int cursor_x = 1;
                int cursor_y = 1;
                int row_height = 0;
                bool dirty = false;
                bool needs_initial_upload = true;
        };

        struct AtlasGlyph {
                uint32_t page = 0;
                uint32_t u = 0;
                uint32_t v = 0;
                uint32_t width = 0;
                uint32_t height = 0;
                int offset_x = 0;
                int offset_y = 0;
        };

        struct GlyphAtlas {
                const Font* font = nullptr;
                float size_px = 0.0f;
                float scale = 1.0f; // stb scale (pixels per font unit)
                std::vector<GlyphPage> pages{};
                std::unordered_map<uint32_t, AtlasGlyph> glyphs{};
        };

        auto TransformX(float x) const -> float;
        auto TransformY(float y) const -> float;
        auto TransformRect(const Rect& world) const -> Rect;
        auto BuildViewProjection(float out_width, float out_height) const -> std::array<float, 16>;
        auto ApplyPostProcess(const PostProcess2DParams& params) -> void;

        auto EnsureGlyph(GlyphAtlas& atlas, uint32_t codepoint) -> const AtlasGlyph*;
        auto ExpandTextOp(const DrawOp& op, std::vector<DrawOp>& expanded) -> void;

        // Returns the glyph layout for `text` (relative to origin), computing
        // it once and reusing it across frames. Shared by MeasureText and
        // ExpandTextOp so a measurement can never disagree with what is drawn,
        // and repeated draws of the same text reuse one layout.
        //
        // Interim: this is a transparent memoization inside Renderer2D. The
        // long-term contract is an explicit immutable TextLayout object owned
        // by the caller (see Render/Text/TextLayout-Design-CN.md); this cache
        // is replaced by that once TextShaper/TextLayout land.
        auto GetOrLayoutText(const Font& font, std::string_view text, float size_px, float max_width) const
            -> const TextLayoutResult&;

        IRenderDevice* device_ = nullptr;
        IRender2DContext* context_ = nullptr;
        // Shared with every texture handle this renderer hands out; see
        // TexturePoolState. Replaced by Initialize(), invalidated by Shutdown().
        std::shared_ptr<TexturePoolState> texture_pool_state_ = std::make_shared<TexturePoolState>();
        render::Sampler2D sampler_ = render::invalid_sampler_2d;
        // Glyph atlas texels are raster coverage data, not colour artwork.
        // Sampling them at exact texel centres avoids a glyph's transparent
        // gutter bleeding into narrow strokes.
        render::Sampler2D glyph_sampler_ = render::invalid_sampler_2d;
        bool initialized_ = false;
        bool in_frame_ = false;
        float origin_x_ = 0.0f;
        float origin_y_ = 0.0f;
        float zoom_ = 1.0f;
        float output_width_ = 0.0f;
        float output_height_ = 0.0f;

        std::vector<DrawOp> ops_{};
        std::vector<Rect> clip_stack_{};
        std::vector<int32_t> layer_stack_{};
        int32_t current_layer_ = 0;

        std::vector<std::unique_ptr<Texture>> textures_{};
        std::vector<std::unique_ptr<Font>> fonts_{};
        std::vector<std::unique_ptr<GlyphAtlas>> atlases_{};
        Texture* white_texture_ = nullptr;

        // Text layout cache (mutable so the const MeasureText can populate it).
        // Bounded; cleared on Shutdown/Initialize because fonts are rebuilt.
        static constexpr std::size_t text_layout_cache_capacity = 128;
        mutable std::unordered_map<TextLayoutKey, TextLayoutResult, TextLayoutKeyHash> text_layout_cache_{};

        struct PendingUpload {
                render::Texture2D handle = render::invalid_texture_2d;
                uint32_t x = 0;
                uint32_t y = 0;
                uint32_t width = 0;
                uint32_t height = 0;
                uint32_t pitch_bytes = 0;
                std::vector<uint8_t> rgba{};
        };
        std::unordered_map<render::Texture2D, PendingUpload> pending_uploads_{};
        std::vector<Texture*> deferred_destroys_{};
        PostProcess2DParams postprocess_params_{};
};

} // namespace atom::render

#endif // ATOM_RENDER_RENDERER2D_HPP

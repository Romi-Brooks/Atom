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

#include <Backend/Contracts/Render/IRender2DContext.hpp>

namespace atom::render {

class Font;
class IRenderDevice;

// High-level batched 2D renderer (Render/ roadmap: Renderer2D). Coordinates
// are logical "world" pixels with a top-left origin and y pointing down; the
// camera (origin + zoom) maps world space onto the viewport. Drawing is
// recorded between BeginFrame()/EndFrame() and flushed in batched GPU
// submissions through the backend's IRender2DContext, so this class never
// touches SDL_GPU / Vulkan types.
class Renderer2D {
    public:
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
                render::Texture2D handle_ = render::kInvalidTexture2D;
                uint32_t width_ = 0;
                uint32_t height_ = 0;
        };

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

        auto SetPostProcess(const PostProcess2DParams& params) -> void;

        // Resources:
        // Creates an RGBA8 texture; rgba (w*h*4 bytes) may be null. The upload
        // happens at the next EndFrame(). Returns nullptr on failure.
        auto CreateTexture(uint32_t width, uint32_t height, const void* rgba = nullptr) -> Texture*;
        auto UpdateTexture(Texture& texture, const void* rgba) -> void;
        auto DestroyTexture(Texture& texture) -> void;

        // Creates a font from TTF/TTC bytes (kept alive by the renderer until
        // DestroyFont() / Shutdown()).
        auto LoadFontFromMemory(std::span<const std::byte> font_data) -> Font*;
        auto DestroyFont(Font* font) -> void;

        [[nodiscard]] auto GetWhiteTexture() const -> Texture*;

        // Drawing (only between BeginFrame() and EndFrame()):
        // dst is in world units; source is an optional texel sub-rectangle
        // (for sprite sheets / glyph atlas pages).
        auto DrawTexture(const Texture& texture, const Rect& dst, const Color& tint = Color::White(),
                         const Rect* source = nullptr) -> void;
        auto DrawRect(const Rect& rect, const Color& color) -> void;
        auto DrawRectOutline(const Rect& rect, const Color& color, float thickness = 1.0f) -> void;
        auto DrawCircle(float center_x, float center_y, float radius, const Color& color, uint32_t segments = 0)
            -> void;
        auto DrawLine(float x0, float y0, float x1, float y1, const Color& color, float thickness = 1.0f) -> void;

        // Text is drawn with its first line's top at (x, y). size_px must be
        // within (0, 512]; max_width == 0 disables line wrapping.
        auto DrawText(const Font& font, std::string_view text, float x, float y, const Color& color, float size_px,
                      float max_width = 0.0f) -> void;

        // Scissor clip in world units (nested). Higher PushLayer values draw
        // later (on top); layer stack is nested.
        auto PushClip(const Rect& rect) -> void;
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
                render::Sampler2D sampler = render::kInvalidSampler2D;
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
        };

        struct AtlasGlyph {
                uint32_t page = 0;
                uint32_t u = 0;
                uint32_t v = 0;
                uint32_t width = 0;
                uint32_t height = 0;
                int offset_x = 0;
                int offset_y = 0;
                float advance = 0.0f;
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

        auto EnsureGlyph(GlyphAtlas& atlas, uint32_t codepoint) -> const AtlasGlyph*;
        auto ExpandTextOp(const DrawOp& op, std::vector<DrawOp>& expanded) -> void;

        IRenderDevice* device_ = nullptr;
        IRender2DContext* context_ = nullptr;
        render::Sampler2D sampler_ = render::kInvalidSampler2D;
        render::Sampler2D sampler_nearest_ = render::kInvalidSampler2D;
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

        struct PendingUpload {
                render::Texture2D handle = render::kInvalidTexture2D;
                std::vector<uint8_t> rgba{};
        };
        std::unordered_map<render::Texture2D, PendingUpload> pending_uploads_{};
        PostProcess2DParams postprocess_params_{};
};

} // namespace atom::render

#endif // ATOM_RENDER_RENDERER2D_HPP

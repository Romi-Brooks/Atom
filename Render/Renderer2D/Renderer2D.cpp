/**
  * @file           : Renderer2D.cpp
  * @author         : Romi Brooks
  * @brief          : Implements the backend-neutral batched 2D renderer.
  * @attention      : Backend-specific encoding is delegated through render contracts.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Renderer2D.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Log/LogSystem.hpp>
#include <Render/Text/Font.hpp>
#include <Render/Core/RenderGraph.hpp>

namespace atom::render {
namespace {

constexpr float kMinZoom = 0.001f;
constexpr float kMaxTextSize = 512.0f;
constexpr uint32_t kPageSize = 1024;
constexpr int kGlyphPadding = 2; // transparent border between glyphs
constexpr float kTwoPi = 6.28318530717958647692f;

auto SameClip(const Rect& a, const Rect& b) -> bool {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

auto NextCodepoint(std::string_view text, std::size_t& index) -> uint32_t {
    if (index >= text.size())
        return 0;
    const auto lead = static_cast<uint8_t>(text[index]);
    if (lead < 0x80) {
        ++index;
        return lead;
    }
    std::size_t length = 0;
    uint32_t codepoint = 0;
    uint32_t minimum = 0;
    if ((lead & 0xE0) == 0xC0) {
        length = 2;
        codepoint = lead & 0x1F;
        minimum = 0x80;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
        codepoint = lead & 0x0F;
        minimum = 0x800;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
        codepoint = lead & 0x07;
        minimum = 0x10000;
    } else
        return 0;
    if (index + length > text.size())
        return 0;
    for (std::size_t k = 1; k < length; ++k) {
        const auto byte = static_cast<uint8_t>(text[index + k]);
        if ((byte & 0xC0) != 0x80)
            return 0;
        codepoint = (codepoint << 6) | (byte & 0x3F);
    }
    if (codepoint < minimum || codepoint > 0x10ffffu || (codepoint >= 0xd800u && codepoint <= 0xdfffu))
        return 0;
    index += length;
    return codepoint;
}

auto ClampColor(const Color& color) -> std::array<float, 4> {
    return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
}

} // namespace

Renderer2D::Renderer2D() = default;

Renderer2D::~Renderer2D() {
    Shutdown();
}

auto Renderer2D::Initialize(IRenderDevice& device, const std::filesystem::path& shader_root) -> bool {
    if (initialized_) {
        if (device_ != &device) {
            LOG_ERROR(atom::render::LogChannel::RENDERER2D, "Renderer2D cannot switch devices without Shutdown");
            return false;
        }
        return true;
    }
    context_ = dynamic_cast<IRender2DContext*>(&device);
    if (!context_ || !context_->Initialize2D(shader_root)) {
        LOG_ERROR(atom::render::LogChannel::RENDERER2D,
                  "Renderer2D: the active device has no usable 2D context (shader root: " + shader_root.string() + ")");
        context_ = nullptr;
        return false;
    }
    device_ = &device;
    sampler_ = context_->CreateSampler2D(
        {Filter2D::Linear, Filter2D::Linear, AddressMode2D::ClampToEdge, AddressMode2D::ClampToEdge});
    if (sampler_ == render::kInvalidSampler2D) {
        LOG_ERROR(atom::render::LogChannel::RENDERER2D, "Renderer2D failed to create its default sampler");
        Shutdown();
        return false;
    }
    sampler_nearest_ = context_->CreateSampler2D(
        {Filter2D::Nearest, Filter2D::Nearest, AddressMode2D::ClampToEdge, AddressMode2D::ClampToEdge});
    if (sampler_nearest_ == render::kInvalidSampler2D) {
        LOG_ERROR(atom::render::LogChannel::RENDERER2D, "Renderer2D failed to create its glyph sampler");
        Shutdown();
        return false;
    }
    white_texture_ = CreateTexture(1, 1, nullptr);
    if (!white_texture_) {
        LOG_ERROR(atom::render::LogChannel::RENDERER2D, "Renderer2D failed to create its white fallback texture");
        Shutdown();
        return false;
    }
    constexpr uint8_t kWhitePixel[4] = {255, 255, 255, 255};
    UpdateTexture(*white_texture_, kWhitePixel);
    initialized_ = true;
    LOG_INFO(atom::render::LogChannel::RENDERER2D,
             "Renderer2D initialized (shader root: " + shader_root.string() + ")");
    return true;
}

auto Renderer2D::IsInitialized() const -> bool {
    return initialized_;
}

auto Renderer2D::Shutdown() -> void {
    if (in_frame_)
        LOG_WARNING(atom::render::LogChannel::RENDERER2D, "Renderer2D shutdown discarded an unfinished frame");
    pending_uploads_.clear();
    atlases_.clear();
    fonts_.clear();
    if (context_) {
        for (auto& texture : textures_) {
            if (texture && texture->handle_ != render::kInvalidTexture2D)
                context_->DestroyTexture2D(texture->handle_);
        }
        if (sampler_ != render::kInvalidSampler2D)
            context_->DestroySampler2D(sampler_);
        if (sampler_nearest_ != render::kInvalidSampler2D)
            context_->DestroySampler2D(sampler_nearest_);
    }
    textures_.clear();
    ops_.clear();
    clip_stack_.clear();
    layer_stack_.clear();
    white_texture_ = nullptr;
    sampler_ = render::kInvalidSampler2D;
    sampler_nearest_ = render::kInvalidSampler2D;
    postprocess_params_ = {};
    context_ = nullptr;
    device_ = nullptr;
    initialized_ = false;
    in_frame_ = false;
}

auto Renderer2D::BeginFrame(const float origin_x, const float origin_y, const float zoom) -> bool {
    if (!initialized_) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D, "Renderer2D::BeginFrame called before initialization");
        return false;
    }
    if (in_frame_) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D, "Renderer2D::BeginFrame called twice without EndFrame");
        return false;
    }
    if (!std::isfinite(origin_x) || !std::isfinite(origin_y) || !std::isfinite(zoom) || zoom <= 0.0f) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D,
                    "Renderer2D::BeginFrame rejected non-finite origin or non-positive zoom");
        return false;
    }
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    zoom_ = std::max(zoom, kMinZoom);
    ops_.clear();
    clip_stack_.clear();
    layer_stack_.clear();
    current_layer_ = 0;
    in_frame_ = true;
    return true;
}

auto Renderer2D::IsInFrame() const -> bool {
    return in_frame_;
}

auto Renderer2D::EndFrame() -> bool {
    if (!initialized_ || !in_frame_ || !device_ || !context_) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D, "Renderer2D::EndFrame called without a valid active frame");
        return false;
    }
    in_frame_ = false;
    if (!clip_stack_.empty() || !layer_stack_.empty())
        LOG_WARNING(atom::render::LogChannel::RENDERER2D,
                    "Renderer2D frame ended with an unbalanced clip or layer stack");
    const auto size = device_->GetOutputSize();
    output_width_ = size.GetX();
    output_height_ = size.GetY();
    if (output_width_ <= 0.0f || output_height_ <= 0.0f) {
        ops_.clear();
        return false;
    }

    // 1) Expand text ops into glyph quads (rasterizing new glyphs into atlas
    //    pages; page uploads are queued into pending_uploads_).
    std::vector<DrawOp> expanded{};
    expanded.reserve(ops_.size() * 2 + 64);
    for (const auto& op : ops_) {
        if (op.kind == OpKind::Text)
            ExpandTextOp(op, expanded);
        else
            expanded.push_back(op);
    }
    ops_.clear();
    // Coalesce glyph insertions: each dirty atlas page is copied to the
    // pending-upload queue at most once per frame.
    for (auto& atlas : atlases_) {
        for (auto& page : atlas->pages) {
            if (page.dirty && page.texture) {
                UpdateTexture(*page.texture, page.rgba.data());
                page.dirty = false;
            }
        }
    }
    std::stable_sort(expanded.begin(), expanded.end(),
                     [](const DrawOp& a, const DrawOp& b) { return a.layer < b.layer; });

    // 2) Build batched geometry into chunks (each chunk respects the backend
    //    vertex/index capacity).
    struct Chunk {
            std::vector<Vertex2D> vertices{};
            std::vector<uint32_t> indices{};
            std::vector<DrawItem2D> items{};
    };
    std::vector<Chunk> chunks{};
    chunks.emplace_back();

    const auto mvp = BuildViewProjection(output_width_, output_height_);

    const Texture* lastTexture = nullptr;
    render::Sampler2D lastSampler = render::kInvalidSampler2D;
    Rect lastClip{};
    bool lastClipEnabled = false;
    bool itemOpen = false;

    auto ensureRoom = [&](std::size_t extraVertices, std::size_t extraIndices) {
        const auto maxVertices = context_->GetMax2DVertices();
        const auto maxIndices = context_->GetMax2DIndices();
        if (chunks.back().vertices.size() + extraVertices > maxVertices ||
            chunks.back().indices.size() + extraIndices > maxIndices) {
            chunks.emplace_back();
            itemOpen = false; // the new chunk has no open item yet
            lastClipEnabled = false;
        }
    };

    for (const auto& op : expanded) {
        const Texture* texture = op.texture ? op.texture : white_texture_;
        const auto sampler = op.sampler != render::kInvalidSampler2D ? op.sampler : sampler_;
        const bool clipEnabled = op.has_clip;
        const Rect clip = clipEnabled ? TransformRect(op.clip) : Rect{};

        const auto beginItem = [&]() {
            const bool same = itemOpen && texture == lastTexture && sampler == lastSampler &&
                              clipEnabled == lastClipEnabled &&
                              (!clipEnabled || SameClip(clip, lastClip));
            if (same)
                return;
            itemOpen = true;
            auto& c = chunks.back();
            c.items.push_back(DrawItem2D{});
            auto& item = c.items.back();
            item.first_index = static_cast<uint32_t>(c.indices.size());
            // Indices are absolute within the chunk. Supplying the item's
            // first vertex here would apply the base offset a second time.
            item.vertex_offset = 0;
            item.texture = texture ? texture->handle_ : white_texture_->handle_;
            item.sampler = sampler;
            if (clipEnabled) {
                item.clip_x = clip.x;
                item.clip_y = clip.y;
                item.clip_w = clip.w;
                item.clip_h = clip.h;
            }
            lastTexture = texture;
            lastSampler = sampler;
            lastClip = clip;
            lastClipEnabled = clipEnabled;
        };

        if (op.kind == OpKind::Quad) {
            float uv0u = 0.0f, uv0v = 0.0f;
            float uv1u = 1.0f, uv1v = 1.0f;
            if (op.has_source && texture && texture->GetWidth() > 0 && texture->GetHeight() > 0) {
                const float tw = static_cast<float>(texture->GetWidth());
                const float th = static_cast<float>(texture->GetHeight());
                uv0u = op.source.x / tw;
                uv0v = op.source.y / th;
                uv1u = (op.source.x + op.source.w) / tw;
                uv1v = (op.source.y + op.source.h) / th;
            }

            ensureRoom(4, 6);
            beginItem();
            // ensureRoom() may have reallocated the chunk vector; rebind now.
            auto& chunk = chunks.back();

            const auto c = ClampColor(op.color);
            const uint32_t base = static_cast<uint32_t>(chunk.vertices.size());
            if (op.custom_quad) {
                const float us[4] = {0.0f, 1.0f, 1.0f, 0.0f};
                const float vs[4] = {0.0f, 0.0f, 1.0f, 1.0f};
                for (std::size_t i = 0; i < 4; ++i) {
                    chunk.vertices.push_back(Vertex2D{c[0], c[1], c[2], c[3], TransformX(op.corners[i][0]),
                                                      TransformY(op.corners[i][1]), us[i], vs[i]});
                }
            } else {
                const float x0 = TransformX(op.dst.x);
                const float y0 = TransformY(op.dst.y);
                const float x1 = TransformX(op.dst.x + op.dst.w);
                const float y1 = TransformY(op.dst.y + op.dst.h);
                chunk.vertices.push_back(Vertex2D{c[0], c[1], c[2], c[3], x0, y0, uv0u, uv0v});
                chunk.vertices.push_back(Vertex2D{c[0], c[1], c[2], c[3], x1, y0, uv1u, uv0v});
                chunk.vertices.push_back(Vertex2D{c[0], c[1], c[2], c[3], x1, y1, uv1u, uv1v});
                chunk.vertices.push_back(Vertex2D{c[0], c[1], c[2], c[3], x0, y1, uv0u, uv1v});
            }
            chunk.indices.push_back(base);
            chunk.indices.push_back(base + 1);
            chunk.indices.push_back(base + 2);
            chunk.indices.push_back(base);
            chunk.indices.push_back(base + 2);
            chunk.indices.push_back(base + 3);
            chunk.items.back().index_count =
                static_cast<uint32_t>(chunk.indices.size()) - chunk.items.back().first_index;
        } else if (op.kind == OpKind::Circle) {
            const uint32_t segments = std::max<uint32_t>(op.segments, 3u);
            ensureRoom(segments + 2, segments * 3);
            beginItem();
            // ensureRoom() may have reallocated the chunk vector; rebind now.
            auto& chunk = chunks.back();
            const auto c = ClampColor(op.color);
            const float cx = TransformX(op.center_x);
            const float cy = TransformY(op.center_y);
            const float radius = op.radius * zoom_;
            const uint32_t base = static_cast<uint32_t>(chunk.vertices.size());
            chunk.vertices.push_back(Vertex2D{c[0], c[1], c[2], c[3], cx, cy, 0.0f, 0.0f});
            for (uint32_t i = 0; i <= segments; ++i) {
                const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
                chunk.vertices.push_back(Vertex2D{c[0], c[1], c[2], c[3], cx + std::cos(angle) * radius,
                                                  cy + std::sin(angle) * radius, 0.0f, 0.0f});
            }
            for (uint32_t i = 0; i < segments; ++i) {
                chunk.indices.push_back(base);
                chunk.indices.push_back(base + i + 1);
                chunk.indices.push_back(base + i + 2);
            }
            chunk.items.back().index_count =
                static_cast<uint32_t>(chunk.indices.size()) - chunk.items.back().first_index;
        }
    }

    // Drop trailing empty items.
    for (auto& chunk : chunks) {
        std::erase_if(chunk.items, [](const DrawItem2D& item) { return item.index_count == 0; });
    }

    // 3) Push all pending uploads (user textures + dirty glyph pages), then
    //    submit every chunk. Uploads run before the first render pass.
    context_->SetPostProcess2D(postprocess_params_);
    bool success = true;
    for (auto upload = pending_uploads_.begin(); upload != pending_uploads_.end();) {
        if (context_->UpdateTexture2D(upload->first, upload->second.rgba.data(), 0)) {
            upload = pending_uploads_.erase(upload);
        } else {
            LOG_ERROR(atom::render::LogChannel::RENDERER2D,
                      "Renderer2D failed to upload texture handle " + std::to_string(upload->first));
            success = false;
            ++upload;
        }
    }

    RenderGraph graph{};
    std::vector<RenderPassId> draw_passes{};
    draw_passes.reserve(chunks.size());
    for (std::size_t chunk_index = 0; chunk_index < chunks.size(); ++chunk_index) {
        const auto* chunk = &chunks[chunk_index];
        if (chunk->vertices.empty() || chunk->indices.empty() || chunk->items.empty())
            continue;
        draw_passes.push_back(graph.AddPass(
            RenderPassDesc{"Renderer2D chunk " + std::to_string(chunk_index), {}},
            [&, chunk]() {
                Render2DFrame frame{};
                frame.view_projection = mvp.data();
                frame.vertices = chunk->vertices.data();
                frame.vertex_count = static_cast<uint32_t>(chunk->vertices.size());
                frame.indices = chunk->indices.data();
                frame.index_count = static_cast<uint32_t>(chunk->indices.size());
                frame.items = chunk->items.data();
                frame.item_count = static_cast<uint32_t>(chunk->items.size());
                if (!context_->Submit2DFrame(frame)) {
                    success = false;
                }
                return true; // keep resolve pass running to release backend state
            }));
    }
    const auto resolve_pass = graph.AddPass(RenderPassDesc{"Renderer2D resolve", draw_passes}, [&]() {
        if (!context_->ResolvePostProcess2D())
            success = false;
        return true;
    });
    if (resolve_pass == kInvalidRenderPass)
        success = false;
    if (!graph.Execute())
        success = false;
    return success;
}

auto Renderer2D::SetPostProcess(const PostProcess2DParams& params) -> void {
    postprocess_params_ = params;
    if (postprocess_params_.has_region &&
        (!std::isfinite(postprocess_params_.region.x) || !std::isfinite(postprocess_params_.region.y) ||
         !std::isfinite(postprocess_params_.region.w) || !std::isfinite(postprocess_params_.region.h) ||
         postprocess_params_.region.w <= 0.0f || postprocess_params_.region.h <= 0.0f)) {
        postprocess_params_.has_region = false;
    }
    if (!std::isfinite(postprocess_params_.amount))
        postprocess_params_.amount = 0.0f;
    if (!std::isfinite(postprocess_params_.scanline))
        postprocess_params_.scanline = 0.0f;
    if (!std::isfinite(postprocess_params_.noise))
        postprocess_params_.noise = 0.0f;
    if (!std::isfinite(postprocess_params_.time))
        postprocess_params_.time = 0.0f;
    if (!std::isfinite(postprocess_params_.direction))
        postprocess_params_.direction = 0.0f;
    postprocess_params_.amount = std::max(0.0f, postprocess_params_.amount);
    postprocess_params_.scanline = std::max(0.0f, postprocess_params_.scanline);
    postprocess_params_.noise = std::max(0.0f, postprocess_params_.noise);
    postprocess_params_.progress = std::clamp(postprocess_params_.progress, 0.0f, 1.0f);
    postprocess_params_.intensity = std::max(0.0f, postprocess_params_.intensity);
}

// --- resources ---------------------------------------------------------------

auto Renderer2D::CreateTexture(const uint32_t width, const uint32_t height, const void* rgba) -> Texture* {
    if (!context_ || width == 0 || height == 0)
        return nullptr;
    const auto max = std::numeric_limits<std::size_t>::max();
    if (static_cast<std::size_t>(height) > max / static_cast<std::size_t>(width) ||
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > max / 4u) {
        LOG_ERROR(atom::render::LogChannel::RENDERER2D,
                  "Renderer2D rejected a texture whose CPU upload size overflows");
        return nullptr;
    }
    auto texture = std::make_unique<Texture>();
    texture->owner_ = this;
    texture->handle_ = context_->CreateTexture2D(width, height);
    if (texture->handle_ == render::kInvalidTexture2D)
        return nullptr;
    texture->width_ = width;
    texture->height_ = height;
    auto* result = texture.get();
    textures_.push_back(std::move(texture));
    if (rgba)
        UpdateTexture(*result, rgba);
    LOG_DEBUG(atom::render::LogChannel::RENDERER2D,
              "Renderer2D created texture " + std::to_string(width) + "x" + std::to_string(height));
    return result;
}

auto Renderer2D::UpdateTexture(Texture& texture, const void* rgba) -> void {
    if (!context_ || texture.owner_ != this || texture.handle_ == render::kInvalidTexture2D || !rgba)
        return;
    const auto count = static_cast<std::size_t>(texture.width_) * texture.height_ * 4u;
    auto& pending = pending_uploads_[texture.handle_];
    pending.handle = texture.handle_;
    pending.rgba.assign(static_cast<const uint8_t*>(rgba), static_cast<const uint8_t*>(rgba) + count);
}

auto Renderer2D::DestroyTexture(Texture& texture) -> void {
    if (in_frame_) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D, "Renderer2D::DestroyTexture ignored during an active frame");
        return;
    }
    if (texture.owner_ != this) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D,
                    "Renderer2D refused to destroy a texture owned by another renderer");
        return;
    }
    const bool wasWhiteTexture = white_texture_ == &texture;
    if (context_ && texture.handle_ != render::kInvalidTexture2D)
        context_->DestroyTexture2D(texture.handle_);
    pending_uploads_.erase(texture.handle_);
    texture.handle_ = render::kInvalidTexture2D;
    texture.owner_ = nullptr;
    std::erase_if(textures_, [&texture](const std::unique_ptr<Texture>& entry) { return entry.get() == &texture; });
    if (wasWhiteTexture)
        white_texture_ = nullptr;
}

auto Renderer2D::LoadFontFromMemory(std::span<const std::byte> font_data) -> Font* {
    auto font = Font::CreateFromMemory(font_data);
    if (!font) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D,
                    "Renderer2D rejected empty, invalid, or unsupported font data");
        return nullptr;
    }
    auto* result = font.get();
    fonts_.push_back(std::move(font));
    LOG_INFO(atom::render::LogChannel::RENDERER2D,
             "Renderer2D loaded font data (bytes=" + std::to_string(font_data.size()) + ")");
    return result;
}

auto Renderer2D::DestroyFont(Font* font) -> void {
    if (!font)
        return;
    if (in_frame_) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D, "Renderer2D::DestroyFont ignored during an active frame");
        return;
    }
    for (const auto& atlas : atlases_) {
        if (atlas->font != font)
            continue;
        for (const auto& page : atlas->pages) {
            if (page.texture)
                DestroyTexture(*page.texture);
        }
    }
    std::erase_if(atlases_, [font](const std::unique_ptr<GlyphAtlas>& atlas) { return atlas->font == font; });
    std::erase_if(fonts_, [font](const std::unique_ptr<Font>& owned) { return owned.get() == font; });
}

auto Renderer2D::GetWhiteTexture() const -> Texture* {
    return white_texture_;
}

// --- recording ---------------------------------------------------------------

auto Renderer2D::DrawTexture(const Texture& texture, const Rect& dst, const Color& tint, const Rect* source) -> void {
    if (!in_frame_ || texture.owner_ != this || texture.handle_ == render::kInvalidTexture2D) {
        LOG_WARNING(atom::render::LogChannel::RENDERER2D,
                    "DrawTexture rejected: in_frame=" + std::to_string(in_frame_) +
                        " owner_match=" + std::to_string(texture.owner_ == this) +
                        " valid_handle=" + std::to_string(texture.handle_ != render::kInvalidTexture2D));
        return;
    }
    DrawOp op{};
    op.layer = current_layer_;
    op.clip = clip_stack_.empty() ? Rect{} : clip_stack_.back();
    op.has_clip = !clip_stack_.empty();
    op.color = tint;
    op.texture = &texture;
    op.dst = dst;
    if (source) {
        op.source = *source;
        op.has_source = true;
    }
    ops_.push_back(std::move(op));
}

auto Renderer2D::DrawRect(const Rect& rect, const Color& color) -> void {
    if (!white_texture_)
        return;
    DrawTexture(*white_texture_, rect, color, nullptr);
}

auto Renderer2D::DrawRectOutline(const Rect& rect, const Color& color, const float thickness) -> void {
    const float t = std::max(thickness, 0.0f);
    if (t <= 0.0f)
        return;
    DrawRect(Rect{rect.x, rect.y, rect.w, t}, color);
    DrawRect(Rect{rect.x, rect.y + rect.h - t, rect.w, t}, color);
    DrawRect(Rect{rect.x, rect.y + t, t, std::max(rect.h - 2.0f * t, 0.0f)}, color);
    DrawRect(Rect{rect.x + rect.w - t, rect.y + t, t, std::max(rect.h - 2.0f * t, 0.0f)}, color);
}

auto Renderer2D::DrawCircle(const float center_x, const float center_y, const float radius, const Color& color,
                            const uint32_t segments) -> void {
    if (!in_frame_ || radius <= 0.0f)
        return;
    DrawOp op{};
    op.kind = OpKind::Circle;
    op.layer = current_layer_;
    op.clip = clip_stack_.empty() ? Rect{} : clip_stack_.back();
    op.has_clip = !clip_stack_.empty();
    op.color = color;
    op.center_x = center_x;
    op.center_y = center_y;
    op.radius = radius;
    op.segments = segments == 0 ? 36u : std::clamp<uint32_t>(segments, 3u, 256u);
    ops_.push_back(std::move(op));
}

auto Renderer2D::DrawLine(const float x0, const float y0, const float x1, const float y1, const Color& color,
                          const float thickness) -> void {
    if (!in_frame_ || !white_texture_ || !std::isfinite(thickness) || thickness <= 0.0f)
        return;
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0f)
        return;
    const float half = std::max(thickness, 0.0f) * 0.5f;
    const float nx = -dy / length * half;
    const float ny = dx / length * half;
    DrawOp op{};
    op.layer = current_layer_;
    op.clip = clip_stack_.empty() ? Rect{} : clip_stack_.back();
    op.has_clip = !clip_stack_.empty();
    op.color = color;
    op.texture = white_texture_;
    op.custom_quad = true;
    op.corners = {{
        {{x0 + nx, y0 + ny}},
        {{x1 + nx, y1 + ny}},
        {{x1 - nx, y1 - ny}},
        {{x0 - nx, y0 - ny}},
    }};
    ops_.push_back(std::move(op));
}

auto Renderer2D::DrawText(const Font& font, const std::string_view text, const float x, const float y,
                          const Color& color, const float size_px, const float max_width) -> void {
    if (!in_frame_ || text.empty() || !font.IsValid() || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(size_px) || !std::isfinite(max_width) || size_px <= 0.0f || size_px > kMaxTextSize ||
        max_width < 0.0f)
        return;
    DrawOp op{};
    op.kind = OpKind::Text;
    op.layer = current_layer_;
    op.clip = clip_stack_.empty() ? Rect{} : clip_stack_.back();
    op.has_clip = !clip_stack_.empty();
    op.color = color;
    op.font = &font;
    op.size_px = size_px;
    op.max_width = max_width;
    op.text.assign(text);
    op.dst = Rect{x, y, 0.0f, 0.0f};
    ops_.push_back(std::move(op));
}

auto Renderer2D::PushClip(const Rect& rect) -> void {
    if (!in_frame_)
        return;
    if (clip_stack_.empty()) {
        clip_stack_.push_back(rect);
        return;
    }
    const auto& parent = clip_stack_.back();
    const float left = std::max(parent.x, rect.x);
    const float top = std::max(parent.y, rect.y);
    const float right = std::min(parent.x + parent.w, rect.x + rect.w);
    const float bottom = std::min(parent.y + parent.h, rect.y + rect.h);
    clip_stack_.push_back(Rect{left, top, std::max(right - left, 0.0f), std::max(bottom - top, 0.0f)});
}
auto Renderer2D::PopClip() -> void {
    if (in_frame_ && !clip_stack_.empty())
        clip_stack_.pop_back();
}
auto Renderer2D::PushLayer(const int32_t layer) -> void {
    if (!in_frame_)
        return;
    layer_stack_.push_back(current_layer_);
    current_layer_ = layer;
}
auto Renderer2D::PopLayer() -> void {
    if (in_frame_ && !layer_stack_.empty()) {
        current_layer_ = layer_stack_.back();
        layer_stack_.pop_back();
    }
}

// --- internals ---------------------------------------------------------------

auto Renderer2D::TransformX(const float x) const -> float {
    return (x - origin_x_) * zoom_;
}
auto Renderer2D::TransformY(const float y) const -> float {
    return (y - origin_y_) * zoom_;
}
auto Renderer2D::TransformRect(const Rect& world) const -> Rect {
    return Rect{TransformX(world.x), TransformY(world.y), world.w * zoom_, world.h * zoom_};
}

auto Renderer2D::BuildViewProjection(const float out_width, const float out_height) const -> std::array<float, 16> {
    std::array<float, 16> m{};
    if (out_width <= 0.0f || out_height <= 0.0f)
        return m;
    // Pixel (top-left origin, y-down) -> NDC: x = 2x/w - 1, y = 1 - 2y/h.
    // Column-major storage: element (row r, col c) lives at c * 4 + r, so the
    // translation column (c = 3) occupies indices 12..15.
    m[0 * 4 + 0] = 2.0f / out_width;
    m[1 * 4 + 1] = -2.0f / out_height;
    m[3 * 4 + 0] = -1.0f;
    m[3 * 4 + 1] = 1.0f;
    m[2 * 4 + 2] = 1.0f;
    m[3 * 4 + 3] = 1.0f;
    return m;
}

auto Renderer2D::EnsureGlyph(GlyphAtlas& atlas, const uint32_t codepoint) -> const AtlasGlyph* {
    const auto existing = atlas.glyphs.find(codepoint);
    if (existing != atlas.glyphs.end())
        return &existing->second;

    AtlasGlyph glyph{};
    glyph.advance = atlas.font ? atlas.font->Advance(codepoint, 0, atlas.scale) : 0.0f;
    if (!atlas.font || !atlas.font->IsValid() || !atlas.font->HasGlyph(codepoint)) {
        glyph.w = 0;
        glyph.h = 0;
        return &atlas.glyphs.emplace(codepoint, glyph).first->second;
    }

    RasterizedGlyph raster{};
    const bool rasterized = atlas.font->Rasterize(codepoint, atlas.scale, raster);
    glyph.w = rasterized ? raster.width : 0;
    glyph.h = rasterized ? raster.height : 0;
    glyph.offset_x = rasterized ? raster.offset_x : 0;
    glyph.offset_y = rasterized ? raster.offset_y : 0;
    if (glyph.w == 0 || glyph.h == 0) {
        glyph.u = 0;
        glyph.v = 0;
        return &atlas.glyphs.emplace(codepoint, glyph).first->second;
    }

    const int gw = static_cast<int>(glyph.w) + kGlyphPadding;
    const int gh = static_cast<int>(glyph.h) + kGlyphPadding;
    if (gw > static_cast<int>(kPageSize) || gh > static_cast<int>(kPageSize))
        return &atlas.glyphs.emplace(codepoint, glyph).first->second;
    std::size_t pageIndex = atlas.pages.size();
    for (std::size_t i = 0; i < atlas.pages.size(); ++i) {
        int candidateX = atlas.pages[i].cursor_x;
        int candidateY = atlas.pages[i].cursor_y;
        if (candidateX + gw > static_cast<int>(atlas.pages[i].size)) {
            candidateX = 1;
            candidateY += atlas.pages[i].row_height;
        }
        if (candidateX + gw <= static_cast<int>(atlas.pages[i].size) &&
            candidateY + gh <= static_cast<int>(atlas.pages[i].size)) {
            pageIndex = i;
            break;
        }
    }
    if (pageIndex == atlas.pages.size()) {
        GlyphPage page{};
        page.size = kPageSize;
        page.texture = CreateTexture(kPageSize, kPageSize, nullptr);
        if (!page.texture)
            return &atlas.glyphs.emplace(codepoint, glyph).first->second;
        page.rgba.assign(static_cast<std::size_t>(kPageSize) * kPageSize * 4u, 0);
        atlas.pages.push_back(std::move(page));
        pageIndex = atlas.pages.size() - 1;
    }
    auto& page = atlas.pages[pageIndex];
    if (page.cursor_x + gw > static_cast<int>(page.size)) {
        page.cursor_x = 1;
        page.cursor_y += page.row_height;
        page.row_height = 0;
    }
    if (page.cursor_y + gh > static_cast<int>(page.size))
        return &atlas.glyphs.emplace(codepoint, glyph).first->second; // oversized glyph

    glyph.page = static_cast<uint32_t>(pageIndex);
    glyph.u = static_cast<uint32_t>(page.cursor_x);
    glyph.v = static_cast<uint32_t>(page.cursor_y);

    const auto* src = raster.rgba.data();
    const std::size_t rowBytes = static_cast<std::size_t>(page.size) * 4u;
    for (uint32_t row = 0; row < glyph.h; ++row) {
        std::memcpy(page.rgba.data() + (static_cast<std::size_t>(page.cursor_y) + row) * rowBytes +
                        static_cast<std::size_t>(page.cursor_x) * 4u,
                    src + static_cast<std::size_t>(row) * glyph.w * 4u, static_cast<std::size_t>(glyph.w) * 4u);
    }
    page.cursor_x += gw;
    page.row_height = std::max(page.row_height, gh);
    page.dirty = true;
    return &atlas.glyphs.emplace(codepoint, glyph).first->second;
}

auto Renderer2D::ExpandTextOp(const DrawOp& op, std::vector<DrawOp>& expanded) -> void {
    if (!op.font || !op.font->IsValid())
        return;
    const float size = op.size_px;

    auto atlasIt = std::find_if(atlases_.begin(), atlases_.end(), [&](const std::unique_ptr<GlyphAtlas>& atlas) {
        return atlas->font == op.font && atlas->size_px == size;
    });
    GlyphAtlas* atlas = nullptr;
    if (atlasIt != atlases_.end()) {
        atlas = atlasIt->get();
    } else {
        auto created = std::make_unique<GlyphAtlas>();
        created->font = op.font;
        created->size_px = size;
        created->scale = op.font->ScaleForPixelHeight(size);
        atlas = created.get();
        atlases_.push_back(std::move(created));
    }

    const auto metrics = op.font->MetricsForPixelHeight(size);
    const float lineHeight = metrics.ascent - metrics.descent + metrics.line_gap;
    const float limit = op.max_width > 0.0f ? op.dst.x + op.max_width : std::numeric_limits<float>::infinity();

    float cursorX = op.dst.x;
    float cursorY = op.dst.y;
    uint32_t previous = 0;
    std::size_t index = 0;
    while (index < op.text.size()) {
        const auto previousIndex = index;
        const uint32_t codepoint = NextCodepoint(op.text, index);
        if (codepoint == 0) {
            // Skip one malformed byte instead of truncating the rest of the line.
            if (index == previousIndex)
                ++index;
            continue;
        }
        if (codepoint == '\n') {
            cursorX = op.dst.x;
            cursorY += lineHeight;
            previous = 0;
            continue;
        }
        const float baseAdvance = op.font->Advance(codepoint, 0, atlas->scale);
        float kerning = op.font->Advance(codepoint, previous, atlas->scale) - baseAdvance;
        if (cursorX + kerning + baseAdvance > limit && cursorX > op.dst.x && codepoint != ' ') {
            cursorX = op.dst.x;
            cursorY += lineHeight;
            kerning = 0.0f; // no kern pair across a wrapped line boundary
        }
        cursorX += kerning;
        previous = codepoint;
        if (codepoint == ' ' || codepoint == '\t') {
            cursorX += codepoint == '\t' ? baseAdvance * 4.0f : baseAdvance;
            continue;
        }
        const AtlasGlyph* glyph = EnsureGlyph(*atlas, codepoint);
        if (!glyph || glyph->w == 0 || glyph->h == 0) {
            cursorX += baseAdvance;
            continue;
        }
        if (glyph->page >= atlas->pages.size())
            continue;
        const auto& page = atlas->pages[glyph->page];
        DrawOp quad{};
        quad.layer = op.layer;
        quad.sampler = sampler_nearest_;
        quad.clip = op.clip;
        quad.has_clip = op.has_clip;
        quad.color = op.color;
        quad.texture = page.texture;
        quad.dst = Rect{cursorX + static_cast<float>(glyph->offset_x),
                        cursorY + metrics.ascent + static_cast<float>(glyph->offset_y), static_cast<float>(glyph->w),
                        static_cast<float>(glyph->h)};
        // Half-texel inset keeps linear sampling away from the transparent
        // gutter between glyphs (avoids edge erosion / garbled small text).
        quad.source = Rect{static_cast<float>(glyph->u) + 0.5f, static_cast<float>(glyph->v) + 0.5f,
                           std::max(static_cast<float>(glyph->w) - 1.0f, 1.0f),
                           std::max(static_cast<float>(glyph->h) - 1.0f, 1.0f)};
        quad.has_source = true;
        expanded.push_back(std::move(quad));
        cursorX += baseAdvance;
    }
}

} // namespace atom::render

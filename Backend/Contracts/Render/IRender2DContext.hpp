/**
  * @file           : IRender2DContext.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral immediate 2D encoding contract.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_IRENDER2DCONTEXT_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_IRENDER2DCONTEXT_HPP

#include <cstdint>
#include <filesystem>

#include <Backend/Contracts/Render/IRenderTypes.hpp>

#include <cstddef>

namespace atom::render {

// Minimal, backend-agnostic 2D rendering surface (RHI slice used by
// Renderer2D). Public headers never expose SDL_GPU / Vulkan types; the render
// backend implements this interface and hands it out through its IRenderDevice
// implementation. A later native Vulkan backend re-implements this same
// contract so Renderer2D can be reused unchanged.

// Batched vertex format used by Renderer2D. Layout must match
// Primitive2D.vert.glsl: straight-alpha vertex color, position (pixels,
// top-left origin, y-down), texture coordinate.
//
// Field order is chosen for D3D12 input-layout alignment: FLOAT4 (color) must
// sit on a 16-byte boundary, so it comes first; the two FLOAT2 attributes
// (position, uv) only need 8-byte alignment and fit at offsets 16 and 24.
struct Vertex2D {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
        float x = 0.0f;
        float y = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
};

static_assert(sizeof(Vertex2D) == 32);
static_assert(offsetof(Vertex2D, r) == 0);
static_assert(offsetof(Vertex2D, x) == 16);
static_assert(offsetof(Vertex2D, u) == 24);

enum class Filter2D { Nearest, Linear };
enum class AddressMode2D { ClampToEdge, Repeat };

struct Sampler2DDesc {
        Filter2D min_filter = Filter2D::Linear;
        Filter2D mag_filter = Filter2D::Linear;
        AddressMode2D address_u = AddressMode2D::ClampToEdge;
        AddressMode2D address_v = AddressMode2D::ClampToEdge;
};

enum class PostProcess2DEffect : uint8_t { None, ChromaticAberration, Glitch, GaussianBlur };

struct PostProcess2DParams {
        PostProcess2DEffect effect = PostProcess2DEffect::None;
        bool has_region = false;
        Rect region{};
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

// Opaque backend resource handles; never expose backend pointers through RHI.
using Texture2D = uint64_t;
using Sampler2D = uint64_t;
inline constexpr Texture2D kInvalidTexture2D = 0;
inline constexpr Sampler2D kInvalidSampler2D = 0;

// One indexed draw inside a submitted 2D frame.
struct DrawItem2D {
        uint32_t first_index = 0;
        uint32_t index_count = 0;
        uint32_t vertex_offset = 0;
        Texture2D texture = kInvalidTexture2D;
        Sampler2D sampler = kInvalidSampler2D;
        // Scissor in framebuffer pixels; negative width disables clipping.
        float clip_x = 0.0f;
        float clip_y = 0.0f;
        float clip_w = -1.0f;
        float clip_h = -1.0f;
};

// A whole batched 2D frame, submitted in one call between IRenderDevice
// BeginFrame() and EndFrame(). The implementation uploads the vertex/index
// data, begins a render pass on the current swapchain texture and draws every
// item in order with a single pipeline. When this frame is the first encoder
// of the frame the pass clears with the device's recorded clear color; later
// encoders load the previous contents (see IRenderDevice::Clear).
struct Render2DFrame {
        const float* view_projection = nullptr; // 16 floats, column-major
        const Vertex2D* vertices = nullptr;
        uint32_t vertex_count = 0;
        const uint32_t* indices = nullptr;
        uint32_t index_count = 0;
        const DrawItem2D* items = nullptr;
        uint32_t item_count = 0;
};

class IRender2DContext {
    public:
        virtual ~IRender2DContext() = default;

        // Loads the built-in 2D pipeline from the compiled shader output
        // directory (e.g. ATOM_SHADER_OUTPUT_DIR). Call once before the first
        // Submit2DFrame.
        virtual auto Initialize2D(const std::filesystem::path& shader_root) -> bool = 0;

        // Creates an empty RGBA8 texture. Returns kInvalidTexture2D on failure.
        virtual auto CreateTexture2D(uint32_t width, uint32_t height) -> Texture2D = 0;
        // Uploads RGBA8 pixels (pitch = bytes per row). Must be called between
        // IRenderDevice::BeginFrame() and EndFrame().
        virtual auto UpdateTexture2D(Texture2D texture, const void* pixels, uint32_t pitch_bytes) -> bool = 0;
        virtual auto DestroyTexture2D(Texture2D texture) -> void = 0;

        virtual auto CreateSampler2D(const Sampler2DDesc& desc) -> Sampler2D = 0;
        virtual auto DestroySampler2D(Sampler2D sampler) -> void = 0;

        // Selects an optional post-process pass for the next submitted frame.
        // The backend renders the batched 2D frame to an offscreen texture and
        // composites it to the swapchain. None keeps the direct path.
        virtual auto SetPostProcess2D(const PostProcess2DParams& params) -> void = 0;
        virtual auto ResolvePostProcess2D() -> bool = 0;

        // Encodes a whole batched frame (uploads, then one render pass with one
        // pipeline). Returns false when no frame is active.
        virtual auto Submit2DFrame(const Render2DFrame& frame) -> bool = 0;

        // Capacity guarantees for a single Render2DFrame.
        [[nodiscard]] virtual auto GetMax2DVertices() const -> uint32_t = 0;
        [[nodiscard]] virtual auto GetMax2DIndices() const -> uint32_t = 0;
};

} // namespace atom::render

#endif // ATOM_BACKEND_CONTRACTS_RENDER_IRENDER2DCONTEXT_HPP

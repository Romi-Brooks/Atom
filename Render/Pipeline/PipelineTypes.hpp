/**
  * @file           : PipelineTypes.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral graphics pipeline descriptions and handles.
  * @attention      : This header must not expose SDL_GPU, Vulkan, D3D or Metal native types.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_RENDER_PIPELINE_PIPELINETYPES_HPP
#define ATOM_RENDER_PIPELINE_PIPELINETYPES_HPP

#include <cstdint>
#include <vector>

namespace atom::render {

// Backend-neutral handles. The backend owns the native pipeline object; the
// frontend only stores and passes these opaque values.
using ShaderProgramHandle = uint64_t;
using PipelineHandle = uint64_t;
inline constexpr ShaderProgramHandle kInvalidShaderProgram = 0;
inline constexpr PipelineHandle kInvalidPipeline = 0;

enum class VertexFormat : uint8_t {
    Float1,
    Float2,
    Float3,
    Float4,
    UNorm8x4,
};

struct VertexAttribute {
    uint32_t location = 0;
    uint32_t offset = 0;
    VertexFormat format = VertexFormat::Float4;
    uint32_t binding = 0;
};

struct VertexLayout {
    uint32_t stride = 0;
    uint32_t binding = 0;
    std::vector<VertexAttribute> attributes{};
};

enum class BlendFactor : uint8_t { Zero, One, SourceAlpha, OneMinusSourceAlpha, DestinationAlpha, OneMinusDestinationAlpha };

struct BlendState {
    bool enabled = false;
    BlendFactor source_color = BlendFactor::One;
    BlendFactor destination_color = BlendFactor::Zero;
    BlendFactor source_alpha = BlendFactor::One;
    BlendFactor destination_alpha = BlendFactor::Zero;
};

enum class CullMode : uint8_t { None, Front, Back };
enum class FrontFace : uint8_t { CounterClockwise, Clockwise };

struct RasterState {
    CullMode cull_mode = CullMode::None;
    FrontFace front_face = FrontFace::CounterClockwise;
    bool scissor_enabled = false;
};

struct DepthStencilState {
    bool depth_test_enabled = false;
    bool depth_write_enabled = false;
};

// The common graphics-pipeline description used by Renderer2D/Renderer3D.
// Backend adapters translate this into SDL_GPU/Vulkan/D3D/Metal state.
struct GraphicsPipelineDesc {
    ShaderProgramHandle vertex_shader = kInvalidShaderProgram;
    ShaderProgramHandle fragment_shader = kInvalidShaderProgram;
    VertexLayout vertex_layout{};
    BlendState blend{};
    RasterState raster{};
    DepthStencilState depth_stencil{};
    uint32_t color_format = 0;
    uint32_t depth_format = 0;
};

} // namespace atom::render

#endif // ATOM_RENDER_PIPELINE_PIPELINETYPES_HPP

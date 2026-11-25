/**
  * @file           : SDLGPU2DPipelineFactory.cpp
  * @author         : Romi Brooks
  * @brief          : Built-in SDL_GPU Renderer2D pipeline creation and release.
  * @attention      : Native SDL_GPU pipeline state remains confined to this backend.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDLGPU2DPipelineFactory.hpp"

#include <array>
#include <string>

#include <Atom/Backend/SDL3/Core/SDLRuntime.hpp>
#include <Atom/Backend/Contracts/Render/IRender2DContext.hpp>
#include <Atom/Core/Log/LogSystem.hpp>

#include "SDLGPUShaderLoader.hpp"

namespace atom::backend::sdlgpu {
namespace {

constexpr std::size_t vertex_stride = sizeof(render::Vertex2D);
static_assert(vertex_stride == 32, "Vertex2D layout must stay a clean 32 bytes");

auto CreatePostProcessPipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat target_format, SDL_GPUShader* vertex,
                               SDL_GPUShader* fragment, const char* label) -> SDL_GPUGraphicsPipeline* {
    if (!vertex || !fragment)
        return nullptr;
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;

    SDL_GPUColorTargetDescription color{};
    color.format = target_format;
    color.blend_state.enable_blend = true;
    color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = false;

    auto* pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!pipeline) {
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  std::string{"Failed to create SDL_GPU post-process pipeline "} + label + ": " + SDL_GetError());
    } else {
        LOG_INFO(atom::log::backend::sdl3::Render,
                 std::string{"Created SDL_GPU post-process pipeline: "} + label);
    }
    return pipeline;
}

} // namespace

auto CreateSDLGPU2DPipelineSet(SDL_GPUDevice* device, const SDL_GPUTextureFormat target_format,
                               const std::filesystem::path& shader_root, SDLGPU2DPipelineSet& out) -> bool {
    if (!device || out.primitive)
        return out.primitive != nullptr;

    auto* primitive_vertex =
        LoadSDLGPUShader(device, shader_root, "Primitive2D.vert.glsl", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    auto* primitive_fragment =
        LoadSDLGPUShader(device, shader_root, "Primitive2D.frag.glsl", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
    if (!primitive_vertex || !primitive_fragment) {
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "SDL_GPU 2D pipeline: required Primitive2D shader variant is missing");
        if (primitive_vertex)
            SDL_ReleaseGPUShader(device, primitive_vertex);
        if (primitive_fragment)
            SDL_ReleaseGPUShader(device, primitive_fragment);
        return false;
    }

    SDL_GPUVertexBufferDescription buffer{0, static_cast<uint32_t>(vertex_stride), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
    const std::array<SDL_GPUVertexAttribute, 3> attributes{{
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, sizeof(float) * 4},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, sizeof(float) * 6},
    }};
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = primitive_vertex;
    info.fragment_shader = primitive_fragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.vertex_input_state = {&buffer, 1, attributes.data(), static_cast<uint32_t>(attributes.size())};
    SDL_GPUColorTargetDescription color{};
    color.format = target_format;
    color.blend_state.enable_blend = true;
    color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;
    out.primitive = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, primitive_vertex);
    SDL_ReleaseGPUShader(device, primitive_fragment);
    if (!out.primitive) {
        LOG_ERROR(atom::log::backend::sdl3::Render,
                  "SDL_GPU Renderer2D pipeline creation failed: " + std::string{SDL_GetError()});
        return false;
    }
    LOG_INFO(atom::log::backend::sdl3::Render, "SDL_GPU Renderer2D primitive pipeline initialized");

    return true;
}

auto GetOrCreateSDLGPUPostProcessPipeline(SDL_GPUDevice* device, const SDL_GPUTextureFormat target_format,
                                       const std::filesystem::path& shader_root,
                                       const render::PostProcess2DEffect effect, SDLGPU2DPipelineSet& pipelines)
    -> SDL_GPUGraphicsPipeline* {
    std::optional<SDL_GPUGraphicsPipeline*>* slot = nullptr;
    const char* label = nullptr;
    switch (effect) {
    case render::PostProcess2DEffect::ChromaticAberration:
        slot = &pipelines.chromatic_aberration;
        label = "ChromaticAberration";
        break;
    case render::PostProcess2DEffect::Glitch:
        slot = &pipelines.glitch;
        label = "Glitch";
        break;
    case render::PostProcess2DEffect::GaussianBlur:
        slot = &pipelines.gaussian_blur;
        label = "GaussianBlur";
        break;
    default:
        return nullptr;
    }
    if (slot->has_value())
        return **slot;

    if (!pipelines.post_vertex.has_value()) {
        pipelines.post_vertex =
            LoadSDLGPUShader(device, shader_root, "PostProcess.vert.glsl", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    }
    *slot = nullptr;
    if (auto* vertex = *pipelines.post_vertex) {
        const std::string filename = std::string{label} + ".frag.glsl";
        if (auto* fragment = LoadSDLGPUShader(device, shader_root, filename.c_str(), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1)) {
            *slot = CreatePostProcessPipeline(device, target_format, vertex, fragment, label);
            SDL_ReleaseGPUShader(device, fragment);
        }
    }
    if (!**slot) {
        LOG_WARNING(atom::log::backend::sdl3::Render, std::string{"Renderer2D effect unavailable: "} +
                        label + "; using direct swapchain rendering");
    }
    return **slot;
}

auto ReleaseSDLGPU2DPipelineSet(SDL_GPUDevice* device, SDLGPU2DPipelineSet& pipelines) -> void {
    if (!device)
        return;
    for (auto* pipeline : {pipelines.primitive, pipelines.chromatic_aberration.value_or(nullptr),
                           pipelines.glitch.value_or(nullptr), pipelines.gaussian_blur.value_or(nullptr)}) {
        if (pipeline)
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    }
    if (auto* vertex = pipelines.post_vertex.value_or(nullptr))
        SDL_ReleaseGPUShader(device, vertex);
    pipelines = {};
}

} // namespace atom::backend::sdlgpu

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

#include <Backend/SDL3/Core/SDLRuntime.hpp>
#include <Backend/Contracts/Render/IRender2DContext.hpp>
#include <Log/LogSystem.hpp>

#include "SDLGPUShaderLoader.hpp"

namespace atom::backend::sdlgpu {
namespace {

constexpr std::size_t kVertexStride = sizeof(render::Vertex2D);
static_assert(kVertexStride == 32, "Vertex2D layout must stay a clean 32 bytes");

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
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  std::string{"Failed to create SDL_GPU post-process pipeline "} + label + ": " + SDL_GetError());
    } else {
        LOG_INFO(atom::backend::sdl3::LogChannel::RENDER,
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
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_GPU 2D pipeline: required Primitive2D shader variant is missing");
        if (primitive_vertex)
            SDL_ReleaseGPUShader(device, primitive_vertex);
        if (primitive_fragment)
            SDL_ReleaseGPUShader(device, primitive_fragment);
        return false;
    }

    SDL_GPUVertexBufferDescription buffer{0, static_cast<uint32_t>(kVertexStride), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
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
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_GPU Renderer2D pipeline creation failed: " + std::string{SDL_GetError()});
        return false;
    }
    LOG_INFO(atom::backend::sdl3::LogChannel::RENDER, "SDL_GPU Renderer2D primitive pipeline initialized");

    auto* post_vertex = LoadSDLGPUShader(device, shader_root, "PostProcess.vert.glsl", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    auto* chromatic =
        LoadSDLGPUShader(device, shader_root, "ChromaticAberration.frag.glsl", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    auto* glitch = LoadSDLGPUShader(device, shader_root, "Glitch.frag.glsl", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    auto* blur = LoadSDLGPUShader(device, shader_root, "GaussianBlur.frag.glsl", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    out.chromatic_aberration = CreatePostProcessPipeline(device, target_format, post_vertex, chromatic, "ChromaticAberration");
    out.glitch = CreatePostProcessPipeline(device, target_format, post_vertex, glitch, "Glitch");
    out.gaussian_blur = CreatePostProcessPipeline(device, target_format, post_vertex, blur, "GaussianBlur");
    if (!out.chromatic_aberration || !out.glitch || !out.gaussian_blur)
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER,
                    "One or more SDL_GPU post-process pipelines are unavailable");
    if (post_vertex)
        SDL_ReleaseGPUShader(device, post_vertex);
    if (chromatic)
        SDL_ReleaseGPUShader(device, chromatic);
    if (glitch)
        SDL_ReleaseGPUShader(device, glitch);
    if (blur)
        SDL_ReleaseGPUShader(device, blur);
    return true;
}

auto ReleaseSDLGPU2DPipelineSet(SDL_GPUDevice* device, SDLGPU2DPipelineSet& pipelines) -> void {
    if (!device)
        return;
    auto release = [device](SDL_GPUGraphicsPipeline*& pipeline) {
        if (pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            pipeline = nullptr;
        }
    };
    release(pipelines.primitive);
    release(pipelines.chromatic_aberration);
    release(pipelines.glitch);
    release(pipelines.gaussian_blur);
}

} // namespace atom::backend::sdlgpu

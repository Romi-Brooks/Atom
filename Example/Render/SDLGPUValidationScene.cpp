/**
  * @file           : SDLGPUValidationScene.cpp
  * @author         : Romi Brooks
  * @brief          : SDL_GPU 2D/3D validation pass implementation.
  * @attention      : Keeps native API calls isolated to the example validation layer.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDLGPUValidationScene.hpp"

#include <array>
#include <cstring>
#include <vector>

#include <Algorithm/Camera/Camera3D.hpp>
#include <Algorithm/Matrix/Mat4.hpp>
#include <Backend/SDLGPU/Device/SDLGPUDevice.hpp>
#include <Backend/SDLGPU/Pipelines/SDLGPUShaderLoader.hpp>
#include <Render/Core/RenderGraph.hpp>

namespace atom::example::render {
using namespace atom::backend::sdlgpu;
namespace {
struct Vertex {
        float position[3];
        float color[3];
};
constexpr std::array<Vertex, 11> vertices{{
    {{-0.8f, -0.7f, 0.0f}, {1.0f, 0.2f, 0.2f}},
    {{0.8f, -0.7f, 0.0f}, {0.2f, 1.0f, 0.2f}},
    {{0.0f, 0.8f, 0.0f}, {0.2f, 0.4f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.1f, 0.1f}},
    {{0.5f, -0.5f, -0.5f}, {0.1f, 1.0f, 0.1f}},
    {{0.5f, 0.5f, -0.5f}, {0.1f, 0.3f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.1f}},
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.1f, 1.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.1f, 1.0f, 1.0f}},
    {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.5f}, {0.2f, 0.2f, 0.2f}},
}};
constexpr std::array<uint16_t, 36> indices{
    {0, 1, 2, 2, 3, 0, 4, 6, 5, 6, 4, 7, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3, 1, 5, 6, 6, 2, 1, 4, 0, 3, 3, 7, 4}};
constexpr std::array<uint32_t, 4> checker{{0xff3030ff, 0xfff0f0ff, 0xfff0f0ff, 0xff3030ff}};

} // namespace

SDLGPUValidationScene::~SDLGPUValidationScene() {
    Shutdown();
}
auto SDLGPUValidationScene::Initialize(SDLGPUDevice& device, const std::filesystem::path& shader_root) -> bool {
    owner_ = &device;
    device_ = device.GetNativeDevice();
    return CreatePipelines(shader_root) && CreateResources();
}

auto SDLGPUValidationScene::CreatePipelines(const std::filesystem::path& root) -> bool {
    auto* meshVertex = LoadSDLGPUShader(device_, root, "MeshUnlit.vert.glsl", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    auto* meshFragment = LoadSDLGPUShader(device_, root, "MeshUnlit.frag.glsl", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
    auto* spriteVertex = LoadSDLGPUShader(device_, root, "Sprite.vert.glsl", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    auto* spriteFragment = LoadSDLGPUShader(device_, root, "Sprite.frag.glsl", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
    if (!meshVertex || !meshFragment || !spriteVertex || !spriteFragment) {
        if (meshVertex)
            SDL_ReleaseGPUShader(device_, meshVertex);
        if (meshFragment)
            SDL_ReleaseGPUShader(device_, meshFragment);
        if (spriteVertex)
            SDL_ReleaseGPUShader(device_, spriteVertex);
        if (spriteFragment)
            SDL_ReleaseGPUShader(device_, spriteFragment);
        return false;
    }
    SDL_GPUColorTargetDescription color{};
    color.format = static_cast<SDL_GPUTextureFormat>(owner_->GetBackendInfo().swapchain_format);
    SDL_GPUVertexBufferDescription bufferDescription{0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
    SDL_GPUVertexAttribute attributes[2]{{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0},
                                         {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, sizeof(float) * 3}};
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = meshVertex;
    info.fragment_shader = meshFragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.vertex_input_state = {&bufferDescription, 1, attributes, 2};
    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    triangle_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    mesh_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    info = {};
    info.vertex_shader = spriteVertex;
    info.fragment_shader = spriteFragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;
    sprite_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    SDL_ReleaseGPUShader(device_, meshVertex);
    SDL_ReleaseGPUShader(device_, meshFragment);
    SDL_ReleaseGPUShader(device_, spriteVertex);
    SDL_ReleaseGPUShader(device_, spriteFragment);
    return triangle_pipeline_ && mesh_pipeline_ && sprite_pipeline_;
}

auto SDLGPUValidationScene::CreateResources() -> bool {
    SDL_GPUBufferCreateInfo vertexInfo{SDL_GPU_BUFFERUSAGE_VERTEX, sizeof(vertices), 0};
    SDL_GPUBufferCreateInfo indexInfo{SDL_GPU_BUFFERUSAGE_INDEX, sizeof(indices), 0};
    vertex_buffer_ = SDL_CreateGPUBuffer(device_, &vertexInfo);
    index_buffer_ = SDL_CreateGPUBuffer(device_, &indexInfo);
    SDL_GPUTextureCreateInfo textureInfo{SDL_GPU_TEXTURETYPE_2D,
                                         SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                         SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                         2,
                                         2,
                                         1,
                                         1,
                                         SDL_GPU_SAMPLECOUNT_1,
                                         0};
    sprite_texture_ = SDL_CreateGPUTexture(device_, &textureInfo);
    SDL_GPUSamplerCreateInfo samplerInfo{};
    samplerInfo.min_filter = samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = samplerInfo.address_mode_v = samplerInfo.address_mode_w =
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_ = SDL_CreateGPUSampler(device_, &samplerInfo);
    if (!vertex_buffer_ || !index_buffer_ || !sprite_texture_ || !sampler_)
        return false;

    const uint32_t vertexOffset = 0, indexOffset = sizeof(vertices), textureOffset = indexOffset + sizeof(indices);
    SDL_GPUTransferBufferCreateInfo transferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, textureOffset + sizeof(checker),
                                                 0};
    auto* transfer = SDL_CreateGPUTransferBuffer(device_, &transferInfo);
    if (!transfer)
        return false;
    auto* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device_, transfer, false));
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        return false;
    }
    std::memcpy(mapped + vertexOffset, vertices.data(), sizeof(vertices));
    std::memcpy(mapped + indexOffset, indices.data(), sizeof(indices));
    std::memcpy(mapped + textureOffset, checker.data(), sizeof(checker));
    SDL_UnmapGPUTransferBuffer(device_, transfer);
    auto* command = SDL_AcquireGPUCommandBuffer(device_);
    if (!command) {
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        return false;
    }
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
        SDL_CancelGPUCommandBuffer(command);
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        return false;
    }
    SDL_GPUTransferBufferLocation vertexSource{transfer, vertexOffset};
    SDL_GPUBufferRegion vertexTarget{vertex_buffer_, 0, sizeof(vertices)};
    SDL_GPUTransferBufferLocation indexSource{transfer, indexOffset};
    SDL_GPUBufferRegion indexTarget{index_buffer_, 0, sizeof(indices)};
    SDL_UploadToGPUBuffer(copy, &vertexSource, &vertexTarget, false);
    SDL_UploadToGPUBuffer(copy, &indexSource, &indexTarget, false);
    SDL_GPUTextureTransferInfo textureSource{transfer, textureOffset, 2, 2};
    SDL_GPUTextureRegion textureTarget{sprite_texture_, 0, 0, 0, 0, 0, 2, 2, 1};
    SDL_UploadToGPUTexture(copy, &textureSource, &textureTarget, false);
    SDL_EndGPUCopyPass(copy);
    const bool submitted = SDL_SubmitGPUCommandBuffer(command);
    SDL_ReleaseGPUTransferBuffer(device_, transfer);
    return submitted;
}

auto SDLGPUValidationScene::EnsureDepthTexture(const uint32_t width, const uint32_t height) -> bool {
    if (width == 0 || height == 0)
        return false;
    if (depth_texture_ && width == depth_width_ && height == depth_height_)
        return true;
    if (depth_texture_)
        SDL_ReleaseGPUTexture(device_, depth_texture_);
    SDL_GPUTextureCreateInfo info{SDL_GPU_TEXTURETYPE_2D,
                                  SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                                  SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                                  width,
                                  height,
                                  1,
                                  1,
                                  SDL_GPU_SAMPLECOUNT_1,
                                  0};
    depth_texture_ = SDL_CreateGPUTexture(device_, &info);
    depth_width_ = width;
    depth_height_ = height;
    return depth_texture_ != nullptr;
}

auto SDLGPUValidationScene::Render(const float elapsed) -> bool {
    const auto size = owner_->GetOutputSize();
    const auto width = static_cast<uint32_t>(size.GetX());
    const auto height = static_cast<uint32_t>(size.GetY());
    auto* command = owner_->GetNativeCommandBuffer();
    auto* swapchain = owner_->GetNativeSwapchainTexture();
    if (!command || !swapchain || !EnsureDepthTexture(width, height))
        return false;
    SDL_GPUColorTargetInfo color{};
    color.texture = swapchain;
    color.clear_color = {0.04f, 0.06f, 0.1f, 1.0f};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = depth_texture_;
    depth.clear_depth = 1.0f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    atom::render::RenderGraph graph{};
    const auto scene_pass = graph.AddPass(
        atom::render::RenderPassDesc{"SDLGPU validation 3D", {}}, [&]() {
            auto* pass = SDL_BeginGPURenderPass(command, &color, 1, &depth);
            if (!pass)
                return false;
            SDL_GPUBufferBinding vertexBinding{vertex_buffer_, 0};
            SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
            SDL_GPUViewport triangleViewport{0, 0, width * 0.5f, height * 0.5f, 0, 1};
            SDL_SetGPUViewport(pass, &triangleViewport);
            SDL_BindGPUGraphicsPipeline(pass, triangle_pipeline_);
            const auto identity = algo::Mat4::Identity();
            SDL_PushGPUVertexUniformData(command, 0, identity.Data(), sizeof(float) * 16);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_GPUViewport cubeViewport{width * 0.5f, 0, width * 0.5f, static_cast<float>(height), 0, 1};
            SDL_SetGPUViewport(pass, &cubeViewport);
            SDL_BindGPUGraphicsPipeline(pass, mesh_pipeline_);
            SDL_GPUBufferBinding indexBinding{index_buffer_, 0};
            SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
            algo::Camera3D camera{{0, 0, -2.5f}, {0, 0, 0}, algo::Vec3::UnitY(), algo::ToRadians(60),
                                  width * 0.5f / height, 0.1f, 10};
            const auto mvp =
                camera.ViewProjectionMatrix() * algo::Mat4::RotationY(elapsed) * algo::Mat4::RotationX(elapsed * 0.6f);
            SDL_PushGPUVertexUniformData(command, 0, mvp.Data(), sizeof(float) * 16);
            SDL_DrawGPUIndexedPrimitives(pass, indices.size(), 1, 0, 3, 0);
            SDL_EndGPURenderPass(pass);
            return true;
        });
    const auto sprite_pass = graph.AddPass(
        atom::render::RenderPassDesc{"SDLGPU validation sprite", {scene_pass}}, [&]() {
            if (scene_pass == atom::render::kInvalidRenderPass)
                return false;
            color.load_op = SDL_GPU_LOADOP_LOAD;
            auto* pass = SDL_BeginGPURenderPass(command, &color, 1, nullptr);
            if (!pass)
                return false;
            SDL_GPUViewport spriteViewport{0, height * 0.55f, width * 0.3f, height * 0.4f, 0, 1};
            SDL_SetGPUViewport(pass, &spriteViewport);
            SDL_BindGPUGraphicsPipeline(pass, sprite_pipeline_);
            SDL_GPUTextureSamplerBinding binding{sprite_texture_, sampler_};
            SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
            SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
            return true;
        });
    const bool success = sprite_pass != atom::render::kInvalidRenderPass && graph.Execute();
    if (success)
        owner_->MarkFrameEncoded();
    return success;
}

auto SDLGPUValidationScene::Shutdown() -> void {
    if (!device_)
        return;
    if (depth_texture_)
        SDL_ReleaseGPUTexture(device_, depth_texture_);
    if (sprite_texture_)
        SDL_ReleaseGPUTexture(device_, sprite_texture_);
    if (sampler_)
        SDL_ReleaseGPUSampler(device_, sampler_);
    if (vertex_buffer_)
        SDL_ReleaseGPUBuffer(device_, vertex_buffer_);
    if (index_buffer_)
        SDL_ReleaseGPUBuffer(device_, index_buffer_);
    if (triangle_pipeline_)
        SDL_ReleaseGPUGraphicsPipeline(device_, triangle_pipeline_);
    if (mesh_pipeline_)
        SDL_ReleaseGPUGraphicsPipeline(device_, mesh_pipeline_);
    if (sprite_pipeline_)
        SDL_ReleaseGPUGraphicsPipeline(device_, sprite_pipeline_);
    *this = {};
}
} // namespace atom::example::render

/**
  * @file           : SDLGPU2D.cpp
  * @author         : Romi Brooks
  * @brief          : Implements SDL_GPU encoding for the 2D render contract.
  * @attention      : SDL_GPU native objects remain private to this backend.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// SDL_GPU implementation of atom::render::IRender2DContext. This is the
// backend half of Renderer2D: it owns the built-in batched 2D pipeline
// (Primitive2D), the per-frame vertex/index staging path and the opaque
// texture/sampler handle maps. No SDL types escape this file's interface.

#include "../Device/SDLGPUDevice.hpp"
#include "../Pipelines/SDLGPU2DPipelineFactory.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include <Log/LogSystem.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>

namespace atom::backend::sdlgpu {
namespace {

constexpr std::size_t kVertexStride = sizeof(render::Vertex2D);
static_assert(kVertexStride == 32, "Vertex2D layout must stay a clean 32 bytes");

constexpr uint32_t kMax2DVertices = 65535;
constexpr uint32_t kMax2DIndices = 196605;

} // namespace

auto SDLGPUDevice::Initialize2D(const std::filesystem::path& shader_root) -> bool {
    if (pipeline_2d_)
        return true;
    if (pipeline_2d_init_failed_)
        return false;
    if (!device_)
        return false;
    shader_root_2d_ = shader_root;

    if (!CreateSDLGPU2DPipelineSet(device_, static_cast<SDL_GPUTextureFormat>(info_.swapchain_format), shader_root,
                                   pipeline_set_)) {
        pipeline_2d_init_failed_ = true;
        return false;
    }
    pipeline_2d_ = pipeline_set_.primitive;
    pipeline_postprocess_ = pipeline_set_.chromatic_aberration;
    pipeline_glitch_ = pipeline_set_.glitch;
    pipeline_blur_ = pipeline_set_.gaussian_blur;
    return true;
}

auto SDLGPUDevice::SetPostProcess2D(const render::PostProcess2DParams& params) -> void {
    postprocess_params_ = params;
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
    if (!std::isfinite(postprocess_params_.corner_radius))
        postprocess_params_.corner_radius = 0.0f;
    if (!std::isfinite(postprocess_params_.feather))
        postprocess_params_.feather = 0.0f;
    postprocess_params_.corner_radius = std::max(0.0f, postprocess_params_.corner_radius);
    postprocess_params_.feather = std::max(0.0f, postprocess_params_.feather);
    postprocess_params_.amount = std::max(0.0f, postprocess_params_.amount);
    postprocess_params_.scanline = std::max(0.0f, postprocess_params_.scanline);
    postprocess_params_.noise = std::max(0.0f, postprocess_params_.noise);
    postprocess_params_.progress = std::clamp(postprocess_params_.progress, 0.0f, 1.0f);
    postprocess_params_.intensity = std::max(0.0f, postprocess_params_.intensity);
}

auto SDLGPUDevice::ResolvePostProcess2D() -> bool {
    if (postprocess_params_.effect == render::PostProcess2DEffect::None || !postprocess_target_encoded_)
        return true;
    if (!EncodePostProcess(postprocess_texture_, postprocess_width_, postprocess_height_)) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER, "Renderer2D post-process pass failed");
        return false;
    }
    frame_encoded_ = true;
    return true;
}

auto SDLGPUDevice::EnsurePostProcessTarget(const uint32_t width, const uint32_t height) -> bool {
    if (postprocess_texture_ && postprocess_width_ == width && postprocess_height_ == height)
        return true;
    if (postprocess_texture_)
        SDL_ReleaseGPUTexture(device_, postprocess_texture_);
    SDL_GPUTextureCreateInfo targetInfo{SDL_GPU_TEXTURETYPE_2D,
                                        static_cast<SDL_GPUTextureFormat>(info_.swapchain_format),
                                        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                        width,
                                        height,
                                        1,
                                        1,
                                        SDL_GPU_SAMPLECOUNT_1,
                                        0};
    postprocess_texture_ = SDL_CreateGPUTexture(device_, &targetInfo);
    if (!postprocess_texture_) {
        postprocess_width_ = postprocess_height_ = 0;
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Failed to create Renderer2D post-process target: " + std::string{SDL_GetError()});
        return false;
    }
    postprocess_width_ = width;
    postprocess_height_ = height;
    return true;
}

auto SDLGPUDevice::EncodePostProcess(SDL_GPUTexture* source, const uint32_t width, const uint32_t height) -> bool {
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    switch (postprocess_params_.effect) {
    case render::PostProcess2DEffect::ChromaticAberration:
        pipeline = pipeline_postprocess_;
        break;
    case render::PostProcess2DEffect::Glitch:
        pipeline = pipeline_glitch_;
        break;
    case render::PostProcess2DEffect::GaussianBlur:
        pipeline = pipeline_blur_;
        break;
    case render::PostProcess2DEffect::None:
        return true;
    }
    if (!pipeline || !source || !swapchain_texture_ || samplers_2d_.empty())
        return false;
    const auto sampler = samplers_2d_.begin()->second;
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain_texture_;
    target.load_op = frame_encoded_ ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
    target.clear_color = {clear_color_.r / 255.0f, clear_color_.g / 255.0f, clear_color_.b / 255.0f,
                          clear_color_.a / 255.0f};
    target.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command_buffer_, &target, 1, nullptr);
    if (!pass)
        return false;
    SDL_GPUViewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_Rect scissor{0, 0, static_cast<int>(width), static_cast<int>(height)};
    if (postprocess_params_.has_region) {
        const auto& region = postprocess_params_.region;
        const auto left = std::clamp(static_cast<int>(std::floor(region.x - postprocess_params_.feather)), 0,
                                     static_cast<int>(width));
        const auto top = std::clamp(static_cast<int>(std::floor(region.y - postprocess_params_.feather)), 0,
                                    static_cast<int>(height));
        const auto right = std::clamp(static_cast<int>(std::ceil(region.x + region.w + postprocess_params_.feather)),
                                      left, static_cast<int>(width));
        const auto bottom =
            std::clamp(static_cast<int>(std::ceil(region.y + region.h + postprocess_params_.feather)), top,
                       static_cast<int>(height));
        scissor = {left, top, right - left, bottom - top};
    }
    if (scissor.w <= 0 || scissor.h <= 0)
        return true;
    SDL_SetGPUScissor(pass, &scissor);
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    SDL_GPUTextureSamplerBinding binding{source, sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    struct alignas(16) Params {
            float a;
            float b;
            float c;
            float d;
            float region_x;
            float region_y;
            float region_w;
            float region_h;
            float corner_radius;
            float feather;
            float unused0;
            float unused1;
    } params{};
    if (postprocess_params_.effect == render::PostProcess2DEffect::ChromaticAberration) {
        params = {postprocess_params_.amount, postprocess_params_.scanline, postprocess_params_.noise,
                  postprocess_params_.time};
    } else if (postprocess_params_.effect == render::PostProcess2DEffect::Glitch) {
        params = {postprocess_params_.progress, postprocess_params_.intensity, postprocess_params_.time,
                  postprocess_params_.direction};
    } else {
        struct alignas(16) BlurParams {
                float radius;
                float corner_radius;
                float feather;
                float unused;
                float region_x;
                float region_y;
                float region_w;
                float region_h;
        } blur{};
        blur.radius = postprocess_params_.amount;
        blur.corner_radius = postprocess_params_.corner_radius;
        blur.feather = postprocess_params_.feather;
        blur.region_x = static_cast<float>(postprocess_params_.region.x) / static_cast<float>(width);
        blur.region_y = static_cast<float>(postprocess_params_.region.y) / static_cast<float>(height);
        blur.region_w = static_cast<float>(postprocess_params_.region.w) / static_cast<float>(width);
        blur.region_h = static_cast<float>(postprocess_params_.region.h) / static_cast<float>(height);
        SDL_PushGPUFragmentUniformData(command_buffer_, 0, &blur, sizeof(blur));
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
        return true;
    }
    const auto& region = postprocess_params_.region;
    if (postprocess_params_.has_region) {
        params.region_x = static_cast<float>(region.x) / static_cast<float>(width);
        params.region_y = static_cast<float>(region.y) / static_cast<float>(height);
        params.region_w = static_cast<float>(region.w) / static_cast<float>(width);
        params.region_h = static_cast<float>(region.h) / static_cast<float>(height);
    } else {
        params.region_x = 0.0f;
        params.region_y = 0.0f;
        params.region_w = 1.0f;
        params.region_h = 1.0f;
    }
    params.corner_radius = postprocess_params_.corner_radius;
    params.feather = postprocess_params_.feather;
    SDL_PushGPUFragmentUniformData(command_buffer_, 0, &params, sizeof(params));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    return true;
}

auto SDLGPUDevice::CreateTexture2D(const uint32_t width, const uint32_t height) -> render::Texture2D {
    if (!device_ || width == 0 || height == 0) {
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER,
                    "CreateTexture2D rejected invalid device or zero-sized texture");
        return render::kInvalidTexture2D;
    }
    SDL_GPUTextureCreateInfo info{SDL_GPU_TEXTURETYPE_2D,
                                  SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                  SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                  width,
                                  height,
                                  1,
                                  1,
                                  SDL_GPU_SAMPLECOUNT_1,
                                  0};
    auto* texture = SDL_CreateGPUTexture(device_, &info);
    if (!texture) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER, "SDL_CreateGPUTexture failed for " + std::to_string(width) +
                                                               "x" + std::to_string(height) + ": " + SDL_GetError());
        return render::kInvalidTexture2D;
    }
    const render::Texture2D handle = next_texture_2d_++;
    textures_2d_.emplace(handle, SDLGPU2DTextureEntry{texture, width, height});
    LOG_DEBUG(atom::backend::sdl3::LogChannel::RENDER, "Created Renderer2D texture handle " + std::to_string(handle) +
                                                           " (" + std::to_string(width) + "x" + std::to_string(height) +
                                                           ")");
    return handle;
}

auto SDLGPUDevice::UpdateTexture2D(const render::Texture2D texture, const void* pixels, const uint32_t pitch_bytes)
    -> bool {
    const auto it = textures_2d_.find(texture);
    if (it == textures_2d_.end() || !pixels) {
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER,
                    "UpdateTexture2D rejected an invalid handle or null pixel buffer");
        return false;
    }
    if (!command_buffer_) {
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER, "UpdateTexture2D requires an active GPU frame");
        return false;
    }
    auto* sdlTexture = it->second.texture;
    if (!sdlTexture)
        return false;
    const uint32_t w = it->second.width;
    const uint32_t h = it->second.height;
    if (w > std::numeric_limits<uint32_t>::max() / 4u) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "UpdateTexture2D rejected a texture whose row size overflows");
        return false;
    }
    const uint32_t rowBytes = w * 4;
    if (h > std::numeric_limits<uint32_t>::max() / rowBytes) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "UpdateTexture2D rejected a texture whose upload size overflows");
        return false;
    }
    if (pitch_bytes != 0 && pitch_bytes < rowBytes) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "UpdateTexture2D rejected a source pitch smaller than one RGBA8 row");
        return false;
    }
    const auto* src = static_cast<const uint8_t*>(pixels);

    SDL_GPUTransferBufferCreateInfo transferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, rowBytes * h, 0};
    auto* transfer = SDL_CreateGPUTransferBuffer(device_, &transferInfo);
    if (!transfer) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_CreateGPUTransferBuffer failed during texture upload: " + std::string{SDL_GetError()});
        return false;
    }
    auto* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device_, transfer, false));
    if (!mapped) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_MapGPUTransferBuffer failed during texture upload: " + std::string{SDL_GetError()});
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        return false;
    }
    const uint32_t sourceStride = pitch_bytes == 0 ? rowBytes : pitch_bytes;
    for (uint32_t y = 0; y < h; ++y)
        std::memcpy(mapped + y * rowBytes, src + y * sourceStride, rowBytes);
    SDL_UnmapGPUTransferBuffer(device_, transfer);

    auto* copy = SDL_BeginGPUCopyPass(command_buffer_);
    if (!copy) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_BeginGPUCopyPass failed during texture upload: " + std::string{SDL_GetError()});
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        return false;
    }
    SDL_GPUTextureTransferInfo source{transfer, 0, w, h};
    SDL_GPUTextureRegion target{sdlTexture, 0, 0, 0, 0, 0, w, h, 1};
    SDL_ClearError();
    SDL_UploadToGPUTexture(copy, &source, &target, true);
    {
        const char* sdlErr = SDL_GetError();
        if (sdlErr && *sdlErr) {
            LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                      "SDL_UploadToGPUTexture reported an error: " + std::string{sdlErr});
        }
    }
    SDL_EndGPUCopyPass(copy);
    // The transfer is still referenced by the open command buffer; release it
    // only after EndFrame() submits (see pending_transfers_2d_).
    pending_transfers_2d_.push_back(transfer);
    return true;
}

auto SDLGPUDevice::DestroyTexture2D(const render::Texture2D texture) -> void {
    const auto it = textures_2d_.find(texture);
    if (it == textures_2d_.end())
        return;
    if (it->second.texture)
        SDL_ReleaseGPUTexture(device_, it->second.texture);
    textures_2d_.erase(it);
}

auto SDLGPUDevice::CreateSampler2D(const render::Sampler2DDesc& desc) -> render::Sampler2D {
    if (!device_)
        return render::kInvalidSampler2D;
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = desc.min_filter == render::Filter2D::Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    info.mag_filter = desc.mag_filter == render::Filter2D::Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = desc.address_u == render::AddressMode2D::Repeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT
                                                                          : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = desc.address_v == render::AddressMode2D::Repeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT
                                                                          : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    auto* sampler = SDL_CreateGPUSampler(device_, &info);
    if (!sampler) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "SDL_CreateGPUSampler failed: " + std::string{SDL_GetError()});
        return render::kInvalidSampler2D;
    }
    const render::Sampler2D handle = next_sampler_2d_++;
    samplers_2d_.emplace(handle, sampler);
    return handle;
}

auto SDLGPUDevice::DestroySampler2D(const render::Sampler2D sampler) -> void {
    const auto it = samplers_2d_.find(sampler);
    if (it == samplers_2d_.end())
        return;
    if (it->second)
        SDL_ReleaseGPUSampler(device_, it->second);
    samplers_2d_.erase(it);
}

auto SDLGPUDevice::Submit2DFrame(const render::Render2DFrame& frame) -> bool {
    if (!device_ || !command_buffer_ || !swapchain_texture_)
        return false;
    if (!pipeline_2d_ && !Initialize2D(shader_root_2d_))
        return false;
    if (frame.vertex_count == 0 || frame.index_count == 0 || !frame.vertices || !frame.indices)
        return true; // nothing to draw; EndFrame() issues the clear pass
    if (!frame.view_projection || (frame.item_count > 0 && !frame.items))
        return false;
    if (frame.vertex_count > kMax2DVertices || frame.index_count > kMax2DIndices) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER, "Renderer2D frame exceeds the backend chunk capacity");
        return false;
    }
    for (uint32_t itemIndex = 0; itemIndex < frame.item_count; ++itemIndex) {
        const auto& item = frame.items[itemIndex];
        if (item.first_index > frame.index_count || item.index_count > frame.index_count - item.first_index) {
            LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                      "Renderer2D rejected an item with an out-of-range index span");
            return false;
        }
        for (uint32_t index = item.first_index; index < item.first_index + item.index_count; ++index) {
            if (frame.indices[index] >= frame.vertex_count ||
                item.vertex_offset > frame.vertex_count - frame.indices[index] - 1) {
                LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                          "Renderer2D rejected an item whose effective vertex index is out of range");
                return false;
            }
        }
    }

    const auto size = GetOutputSize();
    const uint32_t width = static_cast<uint32_t>(size.GetX());
    const uint32_t height = static_cast<uint32_t>(size.GetY());
    if (width == 0 || height == 0)
        return false;
    const bool usePostProcess = postprocess_params_.effect != render::PostProcess2DEffect::None;
    const bool selectedPipelineAvailable = [&] {
        switch (postprocess_params_.effect) {
        case render::PostProcess2DEffect::ChromaticAberration:
            return pipeline_postprocess_ != nullptr;
        case render::PostProcess2DEffect::Glitch:
            return pipeline_glitch_ != nullptr;
        case render::PostProcess2DEffect::GaussianBlur:
            return pipeline_blur_ != nullptr;
        case render::PostProcess2DEffect::None:
            return false;
        }
        return false;
    }();
    if (usePostProcess && (!selectedPipelineAvailable || !EnsurePostProcessTarget(width, height))) {
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER,
                    "Requested Renderer2D post-process effect is unavailable; using direct swapchain rendering");
    }
    const bool postProcessActive = usePostProcess && selectedPipelineAvailable && postprocess_texture_;

    const uint32_t vertexBytes = frame.vertex_count * static_cast<uint32_t>(kVertexStride);
    const uint32_t indexBytes = static_cast<uint32_t>(frame.index_count) * sizeof(uint32_t);

    // Resize the GPU vertex/index buffers when the batch outgrows them.
    if (vertex_capacity_2d_ < frame.vertex_count) {
        if (vertex_buffer_2d_) {
            SDL_ReleaseGPUBuffer(device_, vertex_buffer_2d_);
            vertex_buffer_2d_ = nullptr;
        }
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_VERTEX, vertexBytes, 0};
        vertex_buffer_2d_ = SDL_CreateGPUBuffer(device_, &info);
        vertex_capacity_2d_ = vertex_buffer_2d_ ? frame.vertex_count : 0;
        LOG_DEBUG(atom::backend::sdl3::LogChannel::RENDER,
                  "Renderer2D vertex capacity resized to " + std::to_string(vertex_capacity_2d_));
    }
    if (index_capacity_2d_ < frame.index_count) {
        if (index_buffer_2d_) {
            SDL_ReleaseGPUBuffer(device_, index_buffer_2d_);
            index_buffer_2d_ = nullptr;
        }
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_INDEX, indexBytes, 0};
        index_buffer_2d_ = SDL_CreateGPUBuffer(device_, &info);
        index_capacity_2d_ = index_buffer_2d_ ? frame.index_count : 0;
        LOG_DEBUG(atom::backend::sdl3::LogChannel::RENDER,
                  "Renderer2D index capacity resized to " + std::to_string(index_capacity_2d_));
    }
    if (!vertex_buffer_2d_ || !index_buffer_2d_) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Renderer2D failed to allocate GPU geometry buffers: " + std::string{SDL_GetError()});
        return false;
    }

    SDL_GPUTransferBufferCreateInfo transferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, vertexBytes + indexBytes, 0};
    auto* transfer = SDL_CreateGPUTransferBuffer(device_, &transferInfo);
    if (!transfer) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Renderer2D failed to allocate geometry transfer buffer: " + std::string{SDL_GetError()});
        return false;
    }
    auto* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device_, transfer, false));
    if (!mapped) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Renderer2D failed to map geometry transfer buffer: " + std::string{SDL_GetError()});
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        return false;
    }
    std::memcpy(mapped, frame.vertices, vertexBytes);
    std::memcpy(mapped + vertexBytes, frame.indices, indexBytes);
    SDL_UnmapGPUTransferBuffer(device_, transfer);

    auto* copy = SDL_BeginGPUCopyPass(command_buffer_);
    if (!copy) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Renderer2D failed to begin geometry copy pass: " + std::string{SDL_GetError()});
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        return false;
    }
    SDL_GPUTransferBufferLocation vertexSource{transfer, 0};
    SDL_GPUBufferRegion vertexTarget{vertex_buffer_2d_, 0, vertexBytes};
    SDL_UploadToGPUBuffer(copy, &vertexSource, &vertexTarget, true);
    SDL_GPUTransferBufferLocation indexSource{transfer, vertexBytes};
    SDL_GPUBufferRegion indexTarget{index_buffer_2d_, 0, indexBytes};
    SDL_UploadToGPUBuffer(copy, &indexSource, &indexTarget, true);
    SDL_EndGPUCopyPass(copy);
    // Still referenced by the open command buffer; released after EndFrame()
    // submits (see pending_transfers_2d_).
    pending_transfers_2d_.push_back(transfer);

    SDL_GPUColorTargetInfo target{};
    target.texture = postProcessActive ? postprocess_texture_ : swapchain_texture_;
    const auto& clear = clear_color_;
    if (postProcessActive) {
        // The card pass must not manufacture a full-screen opaque background;
        // transparent texels are composited over the direct background pass.
        target.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        target.clear_color = {clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f};
    }
    target.load_op = postProcessActive ? (postprocess_target_encoded_ ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR)
                                       : (frame_encoded_ ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR);
    target.store_op = SDL_GPU_STOREOP_STORE;

    auto* pass = SDL_BeginGPURenderPass(command_buffer_, &target, 1, nullptr);
    if (!pass) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Renderer2D failed to begin render pass: " + std::string{SDL_GetError()});
        return false;
    }
    SDL_GPUViewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_BindGPUGraphicsPipeline(pass, pipeline_2d_);
    SDL_PushGPUVertexUniformData(command_buffer_, 0, frame.view_projection, sizeof(float) * 16);

    SDL_GPUBufferBinding vertexBinding{vertex_buffer_2d_, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
    SDL_GPUBufferBinding indexBinding{index_buffer_2d_, 0};
    SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    bool allItemsValid = true;
    for (uint32_t i = 0; i < frame.item_count; ++i) {
        const auto& item = frame.items[i];
        if (item.index_count == 0)
            continue;
        const auto textureIt = textures_2d_.find(item.texture);
        const auto samplerIt = samplers_2d_.find(item.sampler);
        if (textureIt == textures_2d_.end() || !textureIt->second.texture || samplerIt == samplers_2d_.end() ||
            !samplerIt->second) {
            LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                      "Renderer2D skipped an item with an invalid texture or sampler handle");
            allItemsValid = false;
            continue;
        }
        SDL_GPUTextureSamplerBinding binding{textureIt->second.texture, samplerIt->second};
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

        if (item.clip_w >= 0.0f) {
            if (!std::isfinite(item.clip_x) || !std::isfinite(item.clip_y) || !std::isfinite(item.clip_w) ||
                !std::isfinite(item.clip_h) || item.clip_h < 0.0f) {
                LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                          "Renderer2D skipped an item with an invalid scissor rectangle");
                allItemsValid = false;
                continue;
            }
            const int framebufferWidth = static_cast<int>(width);
            const int framebufferHeight = static_cast<int>(height);
            const int left = std::clamp(static_cast<int>(std::floor(item.clip_x)), 0, framebufferWidth);
            const int top = std::clamp(static_cast<int>(std::floor(item.clip_y)), 0, framebufferHeight);
            const int right =
                std::clamp(static_cast<int>(std::ceil(item.clip_x + item.clip_w)), left, framebufferWidth);
            const int bottom =
                std::clamp(static_cast<int>(std::ceil(item.clip_y + item.clip_h)), top, framebufferHeight);
            SDL_Rect scissor{left, top, right - left, bottom - top};
            // SDL3 rejects zero-area scissor rectangles (and Vulkan requires a
            // non-zero extent); an empty intersection has no visible pixels.
            if (scissor.w <= 0 || scissor.h <= 0)
                continue;
            SDL_SetGPUScissor(pass, &scissor);
        } else {
            // SDL_GPU has no "disable scissor" call: SDL_SetGPUScissor rejects
            // NULL (see SDL_gpu.c CHECK_PARAM(scissor == NULL)), so passing NULL
            // would silently leave the previous item's scissor bound. Restore
            // the full framebuffer rectangle, matching SDL's own gpu renderer
            // (SDL_render_gpu.c SetViewportAndScissor).
            const SDL_Rect fullScissor{0, 0, static_cast<int>(width), static_cast<int>(height)};
            SDL_SetGPUScissor(pass, &fullScissor);
        }
        SDL_DrawGPUIndexedPrimitives(pass, item.index_count, 1, item.first_index, item.vertex_offset, 0);
    }
    SDL_EndGPURenderPass(pass);
    if (postProcessActive)
        postprocess_target_encoded_ = true;
    else
        frame_encoded_ = true;
    return allItemsValid;
}

auto SDLGPUDevice::GetMax2DVertices() const -> uint32_t {
    return kMax2DVertices;
}
auto SDLGPUDevice::GetMax2DIndices() const -> uint32_t {
    return kMax2DIndices;
}

auto SDLGPUDevice::Release2DResources() -> void {
    if (device_) {
        ReleaseSDLGPU2DPipelineSet(device_, pipeline_set_);
        pipeline_2d_ = nullptr;
        pipeline_postprocess_ = nullptr;
        pipeline_glitch_ = nullptr;
        pipeline_blur_ = nullptr;
        if (postprocess_texture_) {
            SDL_ReleaseGPUTexture(device_, postprocess_texture_);
            postprocess_texture_ = nullptr;
        }
        if (vertex_buffer_2d_) {
            SDL_ReleaseGPUBuffer(device_, vertex_buffer_2d_);
            vertex_buffer_2d_ = nullptr;
        }
        if (index_buffer_2d_) {
            SDL_ReleaseGPUBuffer(device_, index_buffer_2d_);
            index_buffer_2d_ = nullptr;
        }
        for (auto& [handle, entry] : textures_2d_) {
            if (entry.texture)
                SDL_ReleaseGPUTexture(device_, entry.texture);
        }
        for (auto& [handle, sampler] : samplers_2d_) {
            if (sampler)
                SDL_ReleaseGPUSampler(device_, sampler);
        }
        for (auto* transfer : pending_transfers_2d_) {
            if (transfer)
                SDL_ReleaseGPUTransferBuffer(device_, transfer);
        }
    }
    pending_transfers_2d_.clear();
    textures_2d_.clear();
    samplers_2d_.clear();
    next_texture_2d_ = 1;
    next_sampler_2d_ = 1;
    vertex_capacity_2d_ = 0;
    index_capacity_2d_ = 0;
    postprocess_width_ = postprocess_height_ = 0;
    postprocess_params_ = {};
    postprocess_target_encoded_ = false;
    pipeline_2d_init_failed_ = false;
}

} // namespace atom::backend::sdlgpu

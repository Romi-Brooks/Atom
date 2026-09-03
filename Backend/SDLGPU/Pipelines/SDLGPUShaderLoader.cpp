/**
  * @file           : SDLGPUShaderLoader.cpp
  * @author         : Romi Brooks
  * @brief          : SDL_GPU shader variant loading implementation.
  * @attention      : Uses wide-path file access on Windows for non-ASCII assets.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDLGPUShaderLoader.hpp"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include <Log/LogSystem.hpp>

namespace atom::backend::sdlgpu {
namespace {

auto ReadBinary(const std::filesystem::path& path) -> std::vector<uint8_t> {
#ifdef _WIN32
    // Use the wide-character CRT API so shader roots containing non-ASCII
    // characters work on Windows as well as on POSIX platforms.
    auto* file = _wfopen(path.c_str(), L"rb");
#else
    auto* file = std::fopen(path.c_str(), "rb");
#endif
    if (!file)
        return {};
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return {};
    }
    const auto size = std::ftell(file);
    if (size <= 0) {
        std::fclose(file);
        return {};
    }
    std::rewind(file);
    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    const auto read = std::fread(data.data(), 1, data.size(), file);
    std::fclose(file);
    if (read != data.size())
        return {};
    return data;
}

struct Candidate {
    SDL_GPUShaderFormat format;
    const char* directory;
    const char* extension;
    const char* entrypoint;
};

} // namespace

auto LoadSDLGPUShader(SDL_GPUDevice* device, const std::filesystem::path& root, const std::string_view name,
                      const SDL_GPUShaderStage stage, const uint32_t uniform_buffers, const uint32_t samplers)
    -> SDL_GPUShader* {
    if (!device || name.empty())
        return nullptr;

    constexpr std::array<Candidate, 3> candidates{{
        {SDL_GPU_SHADERFORMAT_DXIL, "dxil", ".dxil", "main"},
        {SDL_GPU_SHADERFORMAT_SPIRV, "spirv", ".spv", "main"},
        {SDL_GPU_SHADERFORMAT_MSL, "msl", ".msl", "main0"},
    }};
    const auto supported = SDL_GetGPUShaderFormats(device);
    for (const auto& candidate : candidates) {
        if ((supported & candidate.format) == 0)
            continue;
        const auto path = root / candidate.directory / (std::string{name} + candidate.extension);
        auto code = ReadBinary(path);
        if (code.empty()) {
            LOG_DEBUG(atom::backend::sdl3::LogChannel::RENDER,
                      "SDL_GPU shader variant missing: " + path.string());
            continue;
        }
        if (candidate.format == SDL_GPU_SHADERFORMAT_MSL)
            code.push_back(0);

        SDL_GPUShaderCreateInfo info{};
        info.code = code.data();
        info.code_size = code.size();
        info.entrypoint = candidate.entrypoint;
        info.format = candidate.format;
        info.stage = stage;
        info.num_uniform_buffers = uniform_buffers;
        info.num_samplers = samplers;
        if (auto* shader = SDL_CreateGPUShader(device, &info)) {
            LOG_INFO(atom::backend::sdl3::LogChannel::RENDER,
                     "SDL_GPU loaded shader " + std::string{name} + " from " + path.string());
            return shader;
        }
        LOG_WARNING(atom::backend::sdl3::LogChannel::RENDER,
                    "SDL_GPU shader creation failed for " + path.string() + ": " + SDL_GetError());
    }
    LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
              "SDL_GPU could not load shader " + std::string{name} + " (formats=" + std::to_string(supported) + ")");
    return nullptr;
}

} // namespace atom::backend::sdlgpu

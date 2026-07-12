/**
 * @file           : DecoderRegistry.cpp
 * @author         : Romi Brooks
 * @brief          : Decoder registry — built-in backends register themselves
 * @attention      :
 * @date           : 2026/7/12
  Copyright (c) 2026 Romi Brooks,  All rights reserved.
**/

#include <algorithm>
#include <cctype>

#include <Media/Audio/Decoder/AtomWavDecoderBackend.hpp>

#include <Log/LogSystem.hpp>

#include "DecoderRegistry.hpp"

namespace atom {

// ---------------------------------------------------------------------------
// Static initialisation — registers built-in decoder backends before any
// Load() call runs.  Because DecoderRegistry.cpp is guaranteed to be linked
// (Music / SFXManager both reference it), this runs at program start.
// ---------------------------------------------------------------------------
namespace {
    const bool g_backends_registered = []() {
        DecoderRegistry::Register(".wav", []() -> std::unique_ptr<IAudioDecoder> {
            return std::make_unique<AtomWavDecoderBackend>();
        });
        LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "Registered .wav decoder");
        return true;
    }();
}

// ---------------------------------------------------------------------------
// Registry implementation
// ---------------------------------------------------------------------------

void DecoderRegistry::Register(const std::string& extension, Factory factory)
{
    GetFactories()[extension] = std::move(factory);
    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO,
              "Registered decoder for extension: " + extension);
}

auto DecoderRegistry::Create(const std::string& filepath) -> std::unique_ptr<IAudioDecoder>
{
    // Extract file extension (everything after the last '.')
    const auto dot = filepath.rfind('.');
    if (dot == std::string::npos) {
        LOG_WARNING(atom::LogChannel::SDL_BACKEND_AUDIO,
                    "No extension found in path: " + filepath);
        return nullptr;
    }

    std::string ext = filepath.substr(dot);
    for (auto& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    auto& factories = GetFactories();
    const auto it = factories.find(ext);
    if (it == factories.end()) {
        LOG_WARNING(atom::LogChannel::SDL_BACKEND_AUDIO,
                    "No decoder registered for extension: " + ext);
        return nullptr;
    }

    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO,
              "Creating decoder for: " + filepath + " (ext=" + ext + ")");
    return it->second();
}

auto DecoderRegistry::GetFactories() -> std::unordered_map<std::string, Factory>&
{
    static std::unordered_map<std::string, Factory> factories;
    return factories;
}

} // namespace atom

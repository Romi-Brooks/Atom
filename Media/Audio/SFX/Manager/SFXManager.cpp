/**
  * @file           : SFXManager.cpp
  * @author         : Romi Brooks
  * @brief          : Sound Effect Resource Manager (SDL3 backend)
  * @attention      :
  * @date           : 2025/9/19
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include <cstdint>
#include <fstream>
#include <vector>

#include <SDL3/SDL.h>

#include <Engine/Audio/DecoderRegistry.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Backend/SDL3AudioBuffer.hpp>

#include "SFXManager.hpp"

namespace {

auto BitsPerSampleToFormat(uint16_t bits) -> SDL_AudioFormat {
    switch (bits) {
        case 8:  return SDL_AUDIO_U8;
        case 16: return SDL_AUDIO_S16;
        case 24:
        case 32: return SDL_AUDIO_S32;
        default: return SDL_AUDIO_S16;
    }
}

// Expand packed 24-bit PCM (3 bytes/sample, little-endian signed) to 32-bit
// signed (4 bytes/sample).  SDL3 needs a format it natively understands, and
// this version doesn't define SDL_AUDIO_S24.
void Expand24To32(std::vector<uint8_t>& data, uint64_t num_samples) {
    std::vector<uint8_t> expanded(num_samples * 4);
    auto* src = data.data();
    auto* dst = expanded.data();
    for (uint64_t i = 0; i < num_samples; ++i) {
        int32_t sample = static_cast<int32_t>(src[0])
                       | (static_cast<int32_t>(src[1]) << 8)
                       | (static_cast<int32_t>(src[2]) << 16);
        if (sample & 0x00800000) sample |= 0xFF000000;
        dst[0] = static_cast<uint8_t>(sample & 0xFF);
        dst[1] = static_cast<uint8_t>((sample >> 8) & 0xFF);
        dst[2] = static_cast<uint8_t>((sample >> 16) & 0xFF);
        dst[3] = static_cast<uint8_t>((sample >> 24) & 0xFF);
        src += 3;
        dst += 4;
    }
    data = std::move(expanded);
}

} // anonymous namespace

namespace atom {
    SFXManager& SFXManager::GetManager() {
        static SFXManager manager;
        return manager;
    }

    auto SFXManager::LoadSFXFiles(const std::string& id, const std::string& filePath) -> bool {
        if (sound_buffers_.contains(id)) {
            LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX with id " + id + ", is already loaded");
            return true;
        }

        // Use the decoder registry to decode the file into PCM data
        auto decoder = DecoderRegistry::Create(filePath);
        if (!decoder || !decoder->Open(filePath)) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX,
                      "Failed to open SFX file: " + filePath + ", with id: " + id);
            return false;
        }

        // Make a COPY of the decoder info — Close() zeroes the decoder's internal
        // info_, so keeping a reference would read garbage after Close().
        const auto info = decoder->GetInfo();
        if (info.sample_rate == 0 || info.channels == 0 || info.total_pcm_frames == 0) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX,
                      "Invalid audio info for: " + filePath);
            decoder->Close();
            return false;
        }

        const auto src_bytes_per_frame = static_cast<std::size_t>(info.channels) *
                                         (info.bits_per_sample / 8);
        const auto total_bytes = info.total_pcm_frames * src_bytes_per_frame;

        std::vector<uint8_t> pcm_data(total_bytes);
        const auto decoded = decoder->DecodeChunk(pcm_data.data(),
                                                  static_cast<uint32_t>(total_bytes));
        decoder->Close();

        if (decoded == 0) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX,
                      "No PCM data decoded from: " + filePath);
            return false;
        }

        // For 24-bit, expand to 32-bit since this SDL3 version has no S24
        SDL_AudioFormat fmt = BitsPerSampleToFormat(info.bits_per_sample);
        if (info.bits_per_sample == 24) {
            const auto total_samples = info.total_pcm_frames * info.channels;
            Expand24To32(pcm_data, total_samples);
        }

        // Build the buffer with decoded PCM data and audio spec
        auto buffer = std::make_unique<SDL3AudioBuffer>();
        buffer->LoadFromMemory(pcm_data.data(), static_cast<uint32_t>(pcm_data.size()));

        SDL_AudioFormat fmt_out = fmt;
        // Float WAV → SDL_AUDIO_F32 (BitsPerSampleToFormat returns S32 for 32-bit)
        if (info.is_float && info.bits_per_sample == 32) {
            fmt_out = SDL_AUDIO_F32;
        }
        SDL_AudioSpec spec{};
        spec.format   = fmt_out;
        spec.freq     = static_cast<int>(info.sample_rate);
        spec.channels = static_cast<Uint8>(info.channels);
        buffer->SetAudioSpec(spec);

        sound_buffers_.emplace(id, std::move(buffer));
        LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "Successfully loaded SFX: " + id);

        return true;
    }

    auto SFXManager::GetSFXBuffer(const std::string& id) -> IAudioBuffer* {
        const auto it = sound_buffers_.find(id);
        return (it != sound_buffers_.end()) ? it->second.get() : nullptr;
    }

    auto SFXManager::HasSFX(const std::string& id) const -> bool {
        return sound_buffers_.contains(id);
    }

    auto SFXManager::UnloadSFX(const std::string& id) -> bool {
        const auto it = sound_buffers_.find(id);
        if (it != sound_buffers_.end()) {
            sound_buffers_.erase(it);
            LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "Unloaded SFX: " + id);
            return true;
        }
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX not found for unloading: " + id);
        return false;
    }

    auto SFXManager::UnloadAll() -> void {
        sound_buffers_.clear();
        LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "All SFX resources unloaded");
    }

    auto SFXManager::GetLoadedCount() const -> size_t {
        return sound_buffers_.size();
    }
}

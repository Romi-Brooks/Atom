/**
  * @file           : Music.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include <cstdint>
#include <ranges>
#include <vector>

#include <SDL3/SDL.h>

#include <Engine/Audio/DecoderRegistry.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Backend/SDL3MusicSource.hpp>
#include <Media/Audio/Manager/VolumeManager.hpp>

#include "Music.hpp"

namespace {

// Convert generic bits-per-sample to SDL_AudioFormat.
// This SDL3 version doesn't have an S24 format, so 24-bit samples are
// expanded to S32 (4 bytes/sample) at load time — see ExpandPcm() below.
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
static auto Expand24To32(std::vector<uint8_t>& data, uint64_t num_samples) -> void {
    std::vector<uint8_t> expanded(num_samples * 4);
    auto* src = data.data();
    auto* dst = expanded.data();
    for (uint64_t i = 0; i < num_samples; ++i) {
        // Read 3-byte little-endian signed sample
        int32_t sample = static_cast<int32_t>(src[0])
                       | (static_cast<int32_t>(src[1]) << 8)
                       | (static_cast<int32_t>(src[2]) << 16);
        // Sign-extend 24-bit → 32-bit
        if (sample & 0x00800000) sample |= 0xFF000000;
        // Store as 4-byte little-endian
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
    auto Music::Load(const std::string& id, const std::string& file) -> bool {
        if (musics_.contains(id)) {
            LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "Music with id '" + id + "' is already loaded");
            return true;
        }

        // Use the decoder registry to select the right decoder by extension
        auto decoder = DecoderRegistry::Create(file);
        if (!decoder || !decoder->Open(file)) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_MUSIC,
                      "Failed to open music file: " + file + " for id: " + id);
            return false;
        }

        // Make a COPY of the decoder info — Close() zeroes the decoder's internal
        // info_, so keeping a reference would read garbage after Close().
        const auto info = decoder->GetInfo();
        if (info.sample_rate == 0 || info.channels == 0 || info.total_pcm_frames == 0) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_MUSIC,
                      "Invalid audio info for: " + file);
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
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_MUSIC,
                      "No PCM data decoded from: " + file);
            return false;
        }

        // For 24-bit, expand to 32-bit since this SDL3 version has no S24
        SDL_AudioFormat fmt = BitsPerSampleToFormat(info.bits_per_sample);
        if (info.bits_per_sample == 24) {
            const auto total_samples = info.total_pcm_frames * info.channels;
            Expand24To32(pcm_data, total_samples);
        }

        SDL_AudioFormat fmt_out = fmt;
        // Float WAV → SDL_AUDIO_F32 (BitsPerSampleToFormat can't distinguish int vs float)
        if (info.is_float && info.bits_per_sample == 32) {
            fmt_out = SDL_AUDIO_F32;
        }
        SDL_AudioSpec spec{};
        spec.format   = fmt_out;
        spec.freq     = static_cast<int>(info.sample_rate);
        spec.channels = static_cast<Uint8>(info.channels);

        auto music = std::make_unique<SDL3MusicSource>(std::move(pcm_data), spec);
        musics_[id] = std::move(music);

        LOG_INFO(atom::LogChannel::ATOM_AUDIO_MUSIC, "Successfully loaded music from file for id: " + id);
        return true;
    }

    auto Music::Play(const std::string& id) -> void {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = musics_.find(id);
        if (it != musics_.end() && it->second) {
            it->second->SetVolume(VolumeManager::GetInstance().GetEffectiveMusicVolume());
            it->second->Play();

            {
                std::lock_guard<std::mutex> current_lock(current_playing_mutex_);
                current_playing_id_ = id;
            }

            LOG_INFO(atom::LogChannel::ATOM_AUDIO_MUSIC, "Playing: " + id);
        } else {
            LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "Music not found or not loaded: " + id);
        }
    }

    auto Music::Stop(const std::string& id) -> void {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = musics_.find(id);
        if (it != musics_.end() && it->second) {
            it->second->Stop();

            {
                std::lock_guard<std::mutex> current_lock(current_playing_mutex_);
                if (current_playing_id_ == id) {
                    current_playing_id_.clear();
                }
            }

            LOG_INFO(atom::LogChannel::ATOM_AUDIO_MUSIC, "Stopping: " + id);
        }
    }

    auto Music::SetVolume(const std::string& id, const float volume) -> void {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = musics_.find(id);
        if (it != musics_.end() && it->second) {
            it->second->SetVolume(volume);
        }
    }

    auto Music::Play(const std::string& id, const float volume) -> void {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = musics_.find(id);
        if (it != musics_.end() && it->second) {
            it->second->SetVolume(volume);
            it->second->Play();

            {
                std::lock_guard<std::mutex> current_lock(current_playing_mutex_);
                current_playing_id_ = id;
            }

            LOG_INFO(atom::LogChannel::ATOM_AUDIO_MUSIC, "Playing: " + id + ", with volume: " + std::to_string(volume));
        } else {
            LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "Music not found or not loaded: " + id);
        }
    }

    auto Music::SetMusicVolume(const float volume) -> void {
        VolumeManager::GetInstance().SetMusicVolume(volume);
        for (const auto& val : musics_ | std::views::values) {
            val->SetVolume(volume);
        }
    }

    auto Music::GetMusicVolume() const -> float {
        return VolumeManager::GetInstance().GetMusicVolume();
    }

    auto Music::IsLoaded(const std::string& id) const -> bool {
        const auto it = musics_.find(id);
        return it != musics_.end() && it->second != nullptr;
    }

    auto Music::GetNowPlaying() const -> std::string {
        std::lock_guard<std::mutex> lock(current_playing_mutex_);
        return current_playing_id_;
    }

    auto Music::SetNowPlaying(const std::string& id) -> void {
        std::lock_guard<std::mutex> lock(current_playing_mutex_);
        current_playing_id_ = id;
    }

    auto Music::ClearNowPlaying() -> void {
        std::lock_guard<std::mutex> lock(current_playing_mutex_);
        current_playing_id_.clear();
    }

    auto Music::IsNowPlaying(const std::string& id) const -> bool {
        std::lock_guard<std::mutex> lock(current_playing_mutex_);
        return current_playing_id_ == id;
    }
}

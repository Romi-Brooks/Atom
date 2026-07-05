/**
  * @file           : Music.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include <ranges>
#include <vector>

#include <Log/LogSystem.hpp>
#include <Media/Audio/Manager/VolumeManager.hpp>
#include <Media/Audio/Backend/SDL3MusicSource.hpp>

#include "Music.hpp"

// SDL_LoadWAV is used for WAV decoding; for other formats (MP3, OGG),
// an external decoder (dr_mp3 / libvorbis) should be used.
#include <SDL3/SDL.h>

namespace atom {
    auto Music::Load(const std::string& id, const std::string& file) -> bool {
        if (musics_.contains(id)) {
            LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "Music with id '" + id + "' is already loaded");
            return true;
        }

        // Decode file to PCM data using SDL_LoadWAV
        // Currently supports WAV; extend with dr_mp3/dr_wav for broader format support.
        uint8_t* wav_data = nullptr;
        uint32_t wav_length = 0;
        SDL_AudioSpec spec{};

        if (!SDL_LoadWAV(file.c_str(), &spec, &wav_data, &wav_length)) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_MUSIC, "Failed to load music file: " + file + " for id: " + id);
            return false;
        }

        // Copy PCM data into a vector for the source to own
        std::vector<uint8_t> pcm_data(wav_data, wav_data + wav_length);
        SDL_free(wav_data);

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

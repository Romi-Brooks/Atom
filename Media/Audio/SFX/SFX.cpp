/**
  * @file           : SFX.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/14
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include <ranges>

#include <Log/LogSystem.hpp>
#include <Media/Audio/SFX/Manager/SFXManager.hpp>
#include <Media/Audio/Manager/VolumeManager.hpp>
#include <Media/Audio/Backend/SDL3SFXSource.hpp>

#include "SFX.hpp"

namespace atom {
    auto SFX::Load(const std::string& id, const std::string& filePath) -> bool {
        auto& sfx_manager = SFXManager::GetManager();
        if (!sfx_manager.LoadSFXFiles(id, filePath)) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Error when loading SFX: " + id);
            return false;
        }

        const auto buffer = sfx_manager.GetSFXBuffer(id);
        if (!buffer) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Error getting buffer for SFX: " + id);
            return false;
        }

        // Create SDL3SFXSource and set its PCM buffer
        auto source = std::make_unique<SDL3SFXSource>();
        source->SetBuffer(reinterpret_cast<const uint8_t*>(buffer->GetSamples()),
                          static_cast<uint32_t>(buffer->GetSampleCount() * sizeof(int16_t)));

        // Set the audio spec to match the loaded buffer
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_S16;
        spec.channels = buffer->GetChannelCount();
        spec.freq = static_cast<int>(buffer->GetSampleRate());
        source->SetSpec(spec);

        sounds_[id] = std::move(source);
        return true;
    }

    auto SFX::Play(const std::string& id) -> void {
        const auto it = sounds_.find(id);
        if (it != sounds_.end() && it->second) {
            it->second->SetVolume(VolumeManager::GetInstance().GetEffectiveSfxVolume());
            it->second->Play();
            LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "Playing: " + id);
        } else {
            LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX not found or not loaded: " + id);
        }
    }

    auto SFX::Stop(const std::string& id) -> void {
        auto it = sounds_.find(id);
        if (it != sounds_.end() && it->second) {
            it->second->Stop();
        }
    }

    auto SFX::StopAll() -> void {
        for (auto& it : sounds_ | std::views::values) {
            if (it) {
                it->Stop();
            }
        }
    }

    auto SFX::SetVolume(const std::string& id, const float volume) -> void {
        const auto it = sounds_.find(id);
        if (it != sounds_.end() && it->second) {
            it->second->SetVolume(volume);
        }
    }

    auto SFX::Play(const std::string& id, const float volume) -> void {
        const auto it = sounds_.find(id);
        if (it != sounds_.end() && it->second) {
            it->second->SetVolume(volume);
            it->second->Play();
            LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "Playing: " + id);
        } else {
            LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX not found or not loaded: " + id);
        }
    }

    auto SFX::IsLoaded(const std::string& id) const -> bool {
        const auto it = sounds_.find(id);
        return it != sounds_.end() && it->second != nullptr;
    }

    auto SFX::GetSound(const std::string& id) -> IAudioSource* {
        const auto it = sounds_.find(id);
        return (it != sounds_.end()) ? it->second.get() : nullptr;
    }

    auto SFX::Reset() -> void {
        StopAll();
        sounds_.clear();
    }
}

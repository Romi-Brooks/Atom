/**
  * @file           : SFXManager.cpp
  * @author         : Romi Brooks
  * @brief          : Sound Effect Resource Manager (SDL3 backend)
  * @attention      :
  * @date           : 2025/9/19
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include <fstream>

#include <Log/LogSystem.hpp>

#include "SFXManager.hpp"
#include <Media/Audio/Backend/SDL3AudioBuffer.hpp>

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

        auto buffer = std::make_unique<SDL3AudioBuffer>();
        if (!buffer->LoadFromFile(filePath)) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Failed to load sfx file: " + filePath + ", with id: " + id);
            return false;
        }

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

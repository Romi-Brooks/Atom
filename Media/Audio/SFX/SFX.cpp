/**
  * @file           : SFX.cpp
  * @author         : Romi Brooks
  * @brief          : 
  * @attention      : 
  * @date           : 2025/9/14
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <ranges>

// Third party Library
#include <SFML/Audio/SoundBuffer.hpp>

// Engine Headers
#include <Log/LogSystem.hpp>
#include <Media/Audio/SFX/Manager/SFXManager.hpp>
#include <Media/Audio/Manager/VolumeManager.hpp>

// Self Dependency
#include "SFX.hpp"


namespace atom {
	auto SFX::Load(const std::string& id, const std::string& filePath) -> bool {
		// Load the buffer through the global SFXManager
		auto& sfx_manager = SFXManager::GetManager();
		if (!sfx_manager.LoadSFXFiles(id, filePath)) {
			LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Error when loading SFX: " + id);
			return false;
		}

		// Get the buffer from SFXManager
		const auto buffer = sfx_manager.GetSFXBuffer(id);
		if (!buffer) {
			LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Error getting buffer for SFX: " + id);
			return false;
		}

		// Create a new sound instance
		auto sound = std::make_unique<sf::Sound>(*buffer);

		// Store the sound instance
		sounds_[id] = std::move(sound);

		return true;
	}

	auto SFX::Play(const std::string& id) -> void {
		const auto it = sounds_.find(id);
		if (it != sounds_.end() && it->second) {
			// Read global volume from VolumeManager
			it->second->setVolume(VolumeManager::GetInstance().GetEffectiveSfxVolume());
			it->second->play();
			LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "Playing: " + id);
		} else {
			LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX not found or not loaded: " + id);
		}
	}

	auto SFX::Stop(const std::string& id) -> void {
		auto it = sounds_.find(id);
		if (it != sounds_.end() && it->second) {
			it->second->stop();
		}
	}

	auto SFX::StopAll() -> void {
		for (auto& it : sounds_ | std::views::values) {
			if (it) {
				it->stop();
			}
		}
	}

	auto SFX::SetVolume(const std::string& id, const float volume) -> void {
		const auto it = sounds_.find(id);
		if (it != sounds_.end() && it->second) {
			it->second->setVolume(volume);
		}
	}

	auto SFX::Play(const std::string& id, const float volume) -> void {
		const auto it = sounds_.find(id);
		if (it != sounds_.end() && it->second) {
			it->second->setVolume(volume);
			it->second->play();
			LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "Playing: " + id);
		} else {
			LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX not found or not loaded: " + id);
		}
	}

	auto SFX::IsLoaded(const std::string& id) const -> bool {
		const auto it = sounds_.find(id);
		return it != sounds_.end() && it->second != nullptr;
	}

	auto SFX::GetSound(const std::string& id) -> sf::Sound* {
		const auto it = sounds_.find(id);
		return (it != sounds_.end()) ? it->second.get() : nullptr;
	}

	auto SFX::Reset() -> void {
		StopAll();
		sounds_.clear();
	}
}

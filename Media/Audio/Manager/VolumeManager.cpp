/**
  * @file           : VolumeManager.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Self Dependency
#include "VolumeManager.hpp"

namespace atom {
	VolumeManager& VolumeManager::GetInstance() {
		static VolumeManager instance;
		return instance;
	}

	auto VolumeManager::SetMasterVolume(const float volume) -> void {
		master_volume_ = volume;
	}

	auto VolumeManager::GetMasterVolume() const -> float {
		return master_volume_;
	}

	auto VolumeManager::SetSfxVolume(const float volume) -> void {
		sfx_volume_ = volume;
	}

	auto VolumeManager::GetSfxVolume() const -> float {
		return sfx_volume_;
	}

	auto VolumeManager::SetMusicVolume(const float volume) -> void {
		music_volume_ = volume;
	}

	auto VolumeManager::GetMusicVolume() const -> float {
		return music_volume_;
	}

	auto VolumeManager::GetEffectiveSfxVolume() const -> float {
		return master_volume_ * sfx_volume_ / 100.0f;
	}

	auto VolumeManager::GetEffectiveMusicVolume() const -> float {
		return master_volume_ * music_volume_ / 100.0f;
	}
}

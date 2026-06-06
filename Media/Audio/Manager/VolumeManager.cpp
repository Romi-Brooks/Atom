/**
  * @file           : VolumeManager.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Engine Headers
#include <Media/Audio/Music.hpp>
#include <Media/Audio/SFX.hpp>

// Self Dependency
#include "VolumeManager.hpp"

float atom::SFX::sfx_volume_ = 100.0f;
float atom::Music::music_volume_ = 100.0f;

namespace atom {
	auto VolumeManager::SetSfxVolume(const float volume) -> void {
		SFX::SetSfxVolume(volume);
	}

	auto VolumeManager::GetSfxVolume() -> float {
		return SFX::GetSfxVolume();
	}

	auto VolumeManager::SetMusicVolume(const float volume) -> void {
		Music::GetInstance().SetMusicVolume(volume);
	}

	auto VolumeManager::GetMusicVolume() -> float {
		return Music::GetInstance().GetMusicVolume();
	}

}

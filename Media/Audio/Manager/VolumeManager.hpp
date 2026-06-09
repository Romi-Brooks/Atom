/**
  * @file           : VolumeManager.hpp
  * @author         : Romi Brooks
  * @brief          : Global volume manager (singleton)
  * @attention      : Manages master volume and per-category volume for SFX and Music.
  *                   Effective volume = master × category / 100.
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_VOLUMEMANAGER_HPP
#define ATOM_VOLUMEMANAGER_HPP

namespace atom {
	class VolumeManager {
	private:
		float master_volume_ = 100.0f;
		float sfx_volume_ = 100.0f;
		float music_volume_ = 100.0f;

		VolumeManager() = default;

	public:
		VolumeManager(const VolumeManager&) = delete;
		VolumeManager& operator=(const VolumeManager&) = delete;

		[[nodiscard]] static auto GetInstance() -> VolumeManager&;

		// Master volume (affects all audio)
		// 全局总音量（影响所有音频）
		auto SetMasterVolume(float volume) -> void;
		[[nodiscard]] auto GetMasterVolume() const -> float;

		// Per-category volume
		// 分类音量
		auto SetSfxVolume(float volume) -> void;
		[[nodiscard]] auto GetSfxVolume() const -> float;

		auto SetMusicVolume(float volume) -> void;
		[[nodiscard]] auto GetMusicVolume() const -> float;

		// Effective volume = master × category / 100
		// 实际生效音量 = 总音量 × 分类音量 / 100
		[[nodiscard]] auto GetEffectiveSfxVolume() const -> float;
		[[nodiscard]] auto GetEffectiveMusicVolume() const -> float;
	};
}

#endif // ATOM_VOLUMEMANAGER_HPP

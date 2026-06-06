/**
  * @file           : VolumeManager.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_VOLUMEMANAGER_HPP
#define ATOM_VOLUMEMANAGER_HPP

namespace atom {
	class VolumeManager {
		private:
			VolumeManager() = default;

		public:

			static auto SetSfxVolume(float volume) -> void;

			[[nodiscard]] static auto GetSfxVolume() -> float;

			static auto SetMusicVolume(float volume) -> void;

			[[nodiscard]] static auto GetMusicVolume() -> float;

	};
}



#endif // ATOM_VOLUMEMANAGER_HPP

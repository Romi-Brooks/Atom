/**
  * @file           : SFXManager.hpp
  * @author         : Romi Brooks
  * @brief          : Sound Effect Resource Manager
  * @attention      : We use a singleton class manager to properly guide the "load", "unload" and "get" operations
  *					  of the SFX to ensure that the SFX is correctly and logically played by the engine.
  *					  playback by the engine in a logical and practical way.
  * @date           : 2025/9/19
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_SFXMANAGER_HPP
#define ATOM_SFXMANAGER_HPP

// Standard Library
#include <string>
#include <unordered_map>
#include <memory>

// Third party Library
#include <SFML/Audio/SoundBuffer.hpp>

namespace atom {
	class SFXManager {
		private:
		    std::unordered_map<std::string, std::unique_ptr<sf::SoundBuffer>> sound_buffers_;

		    SFXManager() = default;

		public:
		    // Removing Copy Constructors and Assignment Operators
		    SFXManager(const SFXManager&) = delete;
		    SFXManager& operator=(const SFXManager&) = delete;

			// Get a singleton instance
			[[nodiscard]] static auto GetManager() -> SFXManager&;

		    // Load SFX file
		    // 加载SFX文件
		    auto LoadSFXFiles(const std::string& id, const std::string& filePath) -> bool;

		    // Get SFX buffer
		    // 获取SFX缓冲区
		    [[nodiscard]] auto GetSFXBuffer(const std::string& id) -> sf::SoundBuffer*;

		    // Check if SFX is loaded
		    // 检查SFX是否已加载
		    [[nodiscard]] auto HasSFX(const std::string& id) const -> bool;

		    // Unload specific SFX
		    // 卸载特定SFX
		    auto UnloadSFX(const std::string& id) -> bool;

		    // Unload all SFX
		    // 卸载所有SFX
		    auto UnloadAll() -> void;

		    // Get loaded SFX count
		    // 获取已加载SFX数量
		    [[nodiscard]] auto GetLoadedCount() const -> size_t;
		};
}

#endif // ATOM_SFXMANAGER_HPP

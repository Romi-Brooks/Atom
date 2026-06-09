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

// Standard Library
#include <fstream>

// Engine Headers
#include <Log/LogSystem.hpp>

// Self Dependency
#include "SFXManager.hpp"

namespace atom {
	SFXManager& SFXManager::GetManager() {
		static SFXManager manager;
		return manager;
	}

	auto SFXManager::LoadSFXFiles(const std::string& id, const std::string& filePath)-> bool {
		// Check if already loaded
		// 检查是否已加载
		if (sound_buffers_.contains(id)) {
			LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX with id " + id + ", is already loaded");
			return true; // Loaded, considered successful
		}

		// 1. Load from file
		// 1. 从文件加载
		std::ifstream file(filePath, std::ios::binary | std::ios::ate);
		if (!file.is_open()) {
			LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Failed to open sfx file: " + filePath + ", with id: " + id);
			return false;
		}

		// 2. Get file size
		// 2. 获取文件大小
		const std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		// 3. Read file data into vector
		// 3. 读取文件数据到vector
		std::vector<char> data(size);
		if (!file.read(data.data(), size)) {
			LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Failed to read sfx file: " + filePath + " with id: " + id);
			file.close();
			return false;
		}

		file.close(); // File read complete

		// 4. Create SoundBuffer and load from memory
		// 4. 创建SoundBuffer并从内存加载
		auto buffer = std::make_unique<sf::SoundBuffer>();
		if (!buffer->loadFromMemory(data.data(), data.size())) {
			LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Failed to load sfx data from memory for id: " + id);
			return false;
		}

		// 5. Move buffer into map (transfer ownership)
		// 5. 将buffer移动到map中（转移所有权）
		sound_buffers_.emplace(id, std::move(buffer));
		LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "Successfully loaded SFX: " + id);

		return true;
	}

	auto SFXManager::GetSFXBuffer(const std::string& id) -> sf::SoundBuffer* {
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
} // namespace engine::audio

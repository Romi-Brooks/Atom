/**
  * @file           : Music.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <ranges>

// Engine Headers
#include <Log/LogSystem.hpp>
#include <Media/Audio/Manager/VolumeManager.hpp>

// Self Dependency
#include "Music.hpp"

namespace atom {
	auto Music::Load(const std::string& id, const std::string& file) -> bool {
		if (musics_.contains(id)) {
			LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "Music with id '" + id + "' is already loaded");
			return true;
		}

		auto music = std::make_unique<sf::Music>(file);

		if (!music) {
			LOG_ERROR(atom::LogChannel::ATOM_AUDIO_MUSIC, "Failed to load music from file for id: " + id);
			return false;
		}

		musics_[id] = std::move(music);
		LOG_INFO(atom::LogChannel::ATOM_AUDIO_MUSIC, "Successfully loaded music from file for id: " + id);

		return true;
	}

	auto Music::Play(const std::string& id) -> void {
		std::lock_guard<std::mutex> lock(mutex_);
		const auto it = musics_.find(id);
		if (it != musics_.end() && it->second) {
			// Read global volume from VolumeManager
			it->second->setVolume(VolumeManager::GetInstance().GetEffectiveMusicVolume());
			it->second->play();

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
			it->second->stop();

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
			it->second->setVolume(volume);
		}
	}

	auto Music::Play(const std::string& id, const float volume) -> void {
		std::lock_guard<std::mutex> lock(mutex_);
		const auto it = musics_.find(id);
		if (it != musics_.end() && it->second) {
			it->second->setVolume(volume);
			it->second->play();

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
			val->setVolume(volume);
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

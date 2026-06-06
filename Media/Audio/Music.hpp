/**
  * @file           : Music.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_MUSIC_HPP
#define ATOM_MUSIC_HPP

// Standard Library
#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

// Third party Library
#include <SFML/Audio/Music.hpp>

namespace atom {
	// UI Background, Interactive Background...
	class Music {
		private:
			// Music instances mapped by ID
			std::unordered_map<std::string, std::unique_ptr<sf::Music>> musics_ {};

			std::string current_playing_id_;

			std::mutex mutex_;
			mutable std::mutex current_playing_mutex_;

			// Global volume for all sounds
			static float music_volume_;

			Music() = default;

		public:
			// Delete copy constructor and assignment operator
			Music(const Music&) = delete;
			Music& operator=(const Music&) = delete;

			[[nodiscard]] static auto GetInstance() -> Music&;

			// Load a music file
			auto Load(const std::string& id, const std::string& file) -> bool;

			auto Play(const std::string& id) -> void;

			// if, We play a sound effect with special volume
			auto Play(const std::string& id, float volume) -> void;

			auto Stop(const std::string& id) -> void;

			// Set volume for a specific sound effect
			auto SetVolume(const std::string& id, float volume) -> void;

			// Volume Manager use this interface to set the playing time volume
			auto SetMusicVolume(float volume) const -> void;

			auto SetNowPlaying(const std::string& id) -> void;

			[[nodiscard]] auto GetMusicVolume() const -> float;

			[[nodiscard]] auto GetNowPlaying() const -> std::string;

			[[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;

			[[nodiscard]] auto IsNowPlaying(const std::string& id) const -> bool;

			auto ClearNowPlaying() -> void;
	};
}


#endif // ATOM_MUSIC_HPP

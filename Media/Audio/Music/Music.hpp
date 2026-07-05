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

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <Engine/Interfaces/IAudioSource.hpp>

namespace atom {
    class Music {
        private:
            std::unordered_map<std::string, std::unique_ptr<IAudioSource>> musics_ {};
            std::string current_playing_id_;
            std::mutex mutex_;
            mutable std::mutex current_playing_mutex_;

        public:
            Music() = default;

            Music(const Music&) = delete;
            Music& operator=(const Music&) = delete;

            auto Load(const std::string& id, const std::string& file) -> bool;
            auto Play(const std::string& id) -> void;
            auto Play(const std::string& id, float volume) -> void;
            auto Stop(const std::string& id) -> void;
            auto SetVolume(const std::string& id, float volume) -> void;

            auto SetMusicVolume(float volume) -> void;
            [[nodiscard]] auto GetMusicVolume() const -> float;

            auto SetNowPlaying(const std::string& id) -> void;
            [[nodiscard]] auto GetNowPlaying() const -> std::string;
            [[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;
            [[nodiscard]] auto IsNowPlaying(const std::string& id) const -> bool;
            auto ClearNowPlaying() -> void;
    };
}

#endif // ATOM_MUSIC_HPP

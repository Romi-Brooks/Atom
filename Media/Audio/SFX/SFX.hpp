/**
  * @file           : SFX.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/14
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_SFX_HPP
#define ATOM_SFX_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#include <Engine/Interfaces/IAudioSource.hpp>

namespace atom {
    class SFX final {
        private:
            // One "sound entry" holds the shared PCM data + a pool of voices.
            // Each voice is an independent IAudioSource that can play
            // simultaneously with other voices of the same entry.
            struct SFXEntry {
                std::vector<uint8_t> pcmData;
                SDL_AudioSpec spec;
                std::vector<std::unique_ptr<IAudioSource>> voices;
                uint8_t maxVoices = 8;
            };

            std::unordered_map<std::string, std::unique_ptr<SFXEntry>> entries_;

            // Helpers
            static auto ReapFinishedVoices(SFXEntry& entry) -> void;
            static auto AllocateVoice(SFXEntry& entry) -> IAudioSource*;

        public:
            SFX() = default;

            SFX(const SFX&) = delete;
            SFX& operator=(const SFX&) = delete;

            auto Load(const std::string& id, const std::string& filePath) -> bool;
            auto Play(const std::string& id) -> void;
            auto Stop(const std::string& id) -> void;
            auto StopAll() -> void;
            auto SetVolume(const std::string& id, float volume) -> void;
            auto Play(const std::string& id, float volume) -> void;

            [[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;
            [[nodiscard]] auto GetSound(const std::string& id) -> IAudioSource*;

            auto Reset() -> void;
    };
}

#endif // ATOM_SFX_HPP

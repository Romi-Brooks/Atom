/**
  * @file           : SFXManager.hpp
  * @author         : Romi Brooks
  * @brief          : Sound Effect Resource Manager (Interface-based)
  * @attention      : Manages loading, unloading and retrieval of SFX audio buffers.
  * @date           : 2025/9/19
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_SFXMANAGER_HPP
#define ATOM_SFXMANAGER_HPP

#include <memory>
#include <string>
#include <unordered_map>

#include <Engine/Interfaces/IAudioBuffer.hpp>

namespace atom {
    class SFXManager {
        private:
            std::unordered_map<std::string, std::unique_ptr<IAudioBuffer>> sound_buffers_;

            SFXManager() = default;

        public:
            SFXManager(const SFXManager&) = delete;
            SFXManager& operator=(const SFXManager&) = delete;

            [[nodiscard]] static auto GetManager() -> SFXManager&;

            auto LoadSFXFiles(const std::string& id, const std::string& filePath) -> bool;
            [[nodiscard]] auto GetSFXBuffer(const std::string& id) -> IAudioBuffer*;
            [[nodiscard]] auto HasSFX(const std::string& id) const -> bool;
            auto UnloadSFX(const std::string& id) -> bool;
            auto UnloadAll() -> void;
            [[nodiscard]] auto GetLoadedCount() const -> size_t;
    };
}

#endif // ATOM_SFXMANAGER_HPP

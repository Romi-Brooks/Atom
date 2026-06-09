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

// Standard Library
#include <memory>
#include <string>
#include <unordered_map>

// Third party Library
#include <SFML/Audio/Sound.hpp>

namespace atom {
    class SFX final {
	    private:
	        std::unordered_map<std::string, std::unique_ptr<sf::Sound>> sounds_;

	    public:
	        SFX() = default;

	        // Delete copy constructor and assignment operator
	        SFX(const SFX&) = delete;
	        SFX& operator=(const SFX&) = delete;

	        // Load a sound effect
	        auto Load(const std::string& id, const std::string& filePath) -> bool;

	        // Play a sound effect
	        auto Play(const std::string& id) -> void;

	        // Stop a specific sound effect
	        auto Stop(const std::string& id) -> void;

	        // Stop all sound effects
	        auto StopAll() -> void;

    		// Set volume for a specific sound effect
    		auto SetVolume(const std::string& id, float volume) -> void;

    		// Play a sound effect with special volume
    		auto Play(const std::string& id, float volume) -> void;

	        // Check if a sound effect is loaded
	        [[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;

	        [[nodiscard]] auto GetSound(const std::string& id) -> sf::Sound*;

	        auto Reset() -> void;
    };
}

#endif // ATOM_SFX_HPP

/**
  * @file           : Screen.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_SCREEN_HPP
#define ATOM_SCREEN_HPP

// Third Party Library
#include <SFML/Graphics/RenderWindow.hpp>

namespace atom {
	class Screen {
		private:
		public:
			virtual ~Screen() = default;

			virtual auto Render(sf::RenderWindow& window) -> void = 0;
			virtual auto HandleEvent(const sf::Event& event) -> bool = 0;	// Use Boolean return values to ensure events are handled correctly.
			virtual auto Update(float deltaTime) -> void = 0;

			virtual auto OnActivate() -> void {}    // When active
			virtual auto OnDeactivate() -> void {}  // When switch out
	};
}

#endif // ATOM_SCREEN_HPP

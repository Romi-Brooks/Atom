/**
  * @file           : Screen.hpp
  * @author         : Romi Brooks
  * @brief          : Abstract screen base class using engine interfaces
  * @attention      :
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_SCREEN_HPP
#define ATOM_SCREEN_HPP

#include <Engine/Interfaces/IRenderTarget.hpp>
#include <Engine/Interfaces/IRenderWindow.hpp>

namespace atom {
    class Screen {
        public:
            virtual ~Screen() = default;

            virtual auto Render(IRenderTarget& target) -> void = 0;
            virtual auto HandleEvent(const IEvent& event) -> bool = 0;
            virtual auto Update(float deltaTime) -> void = 0;

            virtual auto OnActivate() -> void {}
            virtual auto OnDeactivate() -> void {}
    };
}

#endif // ATOM_SCREEN_HPP

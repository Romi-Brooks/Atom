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

#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>

namespace atom {
class Screen {
    public:
        virtual ~Screen() = default;

        virtual auto Render(atom::render::IRenderDevice& device) -> void = 0;
        virtual auto HandleEvent(const atom::window::IEvent& event) -> bool = 0;
        // Variable-rate update (animations, presentation). See FixedUpdate for
        // deterministic simulation.
        virtual auto Update(float deltaTime) -> void = 0;
        // Fixed-rate simulation update. `deltaTime` is the domain fixed step in
        // seconds (e.g. 1/60). Called 0..N times per host frame (CORE-001).
        virtual auto FixedUpdate(float deltaTime) -> void {}

        virtual auto OnActivate() -> void {}
        virtual auto OnDeactivate() -> void {}
};
} // namespace atom

#endif // ATOM_SCREEN_HPP

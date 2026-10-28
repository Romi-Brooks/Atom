/**
  * @file           : Screen.hpp
  * @author         : Romi Brooks
  * @brief          : Runtime presentation-state lifecycle contract.
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

        // A Screen orchestrates presentation and input for one runtime state.
        // It deliberately sees only IRenderDevice: drawing policy belongs to
        // Renderer2D/UI/Scene2D, never to Runtime or this lifecycle contract.
        virtual auto Render(atom::render::IRenderDevice& device) -> void = 0;
        virtual auto HandleEvent(const atom::window::IEvent& event) -> bool = 0;

        // Presentation-frame boundary, called once per host frame *before* any
        // fixed step is dispatched. `fixed_steps` is how many FixedUpdate calls
        // this frame will make (0..N, CORE-001) and `fixed_delta_seconds` is the
        // duration they use; both are 0 when the host has no game clock.
        //
        // It exists because a Screen that owns a fixed-step world (GameScreen) must
        // run one ordered world frame - Input, FixedUpdate x N, Update, LateUpdate -
        // while the host only knows how to call FixedUpdate N times and Update once
        // (G0.1). Publishing the step budget here lets that Screen batch the steps
        // into a single GameWorld::RunFrame without re-deriving them, and without
        // running Input/Update/LateUpdate N times by accident. A Screen that
        // implements FixedUpdate itself can ignore this hook entirely.
        virtual auto BeginFrame(std::uint32_t fixed_steps, float fixed_delta_seconds) -> void {
            (void)fixed_steps;
            (void)fixed_delta_seconds;
        }

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

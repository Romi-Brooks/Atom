// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file CameraSystem2D.hpp
 * @brief Target follow, dead zone, bounds and shake for 2D camera entities.
 */
#ifndef ATOM_WORLD2D_CAMERASYSTEM2D_HPP
#define ATOM_WORLD2D_CAMERASYSTEM2D_HPP

#include <cstddef>

#include <Algorithm/Vector/Vec2.hpp>
#include <ECS/EntityWorld.hpp>

namespace atom::world2d {

class CameraSystem2D final {
    public:
        // Run in LateUpdate, after gameplay has moved its targets. Viewport is
        // the output size used when a Camera2D does not specify its own.
        [[nodiscard]] static auto Step(ecs::EntityWorld& world, algo::Vec2 output_viewport,
                                       float delta_seconds) -> std::size_t;
        [[nodiscard]] static auto Shake(ecs::EntityWorld& world, ecs::Entity camera,
                                        float amplitude, float duration_seconds) -> bool;
        // A scripted pan releases the follow target and moves the camera's
        // simulation position with a smoothstep curve. Bounds still apply.
        [[nodiscard]] static auto PanTo(ecs::EntityWorld& world, ecs::Entity camera,
                                        algo::Vec2 target, float duration_seconds) -> bool;
};

} // namespace atom::world2d

#endif // ATOM_WORLD2D_CAMERASYSTEM2D_HPP

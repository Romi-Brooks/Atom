/**
  * @file           : Camera2D.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral 2D camera value type.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ALGORITHM_CAMERA2D_HPP
#define ATOM_ALGORITHM_CAMERA2D_HPP

#include <Algorithm/Matrix/Mat3.hpp>
#include <Algorithm/Vector/Vec2.hpp>

namespace atom::algo {

struct Camera2D {
        Vec2 position = Vec2::Zero();
        Vec2 viewport_size = Vec2::Zero();
        float rotation = 0.0f;
        float zoom = 1.0f;

        [[nodiscard]] auto ViewMatrix() const -> Mat3;
        [[nodiscard]] auto WorldToScreen(const Vec2& point) const -> Vec2;
        [[nodiscard]] auto ScreenToWorld(const Vec2& point) const -> Vec2;
};

} // namespace atom::algo

#endif

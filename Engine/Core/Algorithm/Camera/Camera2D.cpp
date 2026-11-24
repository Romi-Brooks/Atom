/**
  * @file           : Camera2D.cpp
  * @author         : Romi Brooks
  * @brief          : Implements 2D camera view and projection helpers.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Camera2D.hpp"

#include <stdexcept>

namespace atom::algo {

auto Camera2D::ViewMatrix() const -> Mat3 {
    if (zoom <= 0.0f)
        throw std::invalid_argument("Camera2D zoom must be positive");
    return Mat3::Translation(viewport_size * 0.5f) * Mat3::Scale({zoom, zoom}) * Mat3::Rotation(-rotation) *
           Mat3::Translation(-position);
}

auto Camera2D::WorldToScreen(const Vec2& point) const -> Vec2 {
    return ViewMatrix().TransformPoint(point);
}

auto Camera2D::ScreenToWorld(const Vec2& point) const -> Vec2 {
    const auto inverse = ViewMatrix().Inverse();
    if (!inverse)
        throw std::runtime_error("Camera2D view matrix is not invertible");
    return inverse->TransformPoint(point);
}

} // namespace atom::algo

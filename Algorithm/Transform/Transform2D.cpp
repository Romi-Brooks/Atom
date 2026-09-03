/**
  * @file           : Transform2D.cpp
  * @author         : Romi Brooks
  * @brief          : Implements 2D transform composition and point mapping.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Transform2D.hpp"

namespace atom::algo {

auto Transform2D::ToMatrix() const -> Mat3 {
    return Mat3::Translation(position) * Mat3::Rotation(rotation) * Mat3::Scale(scale) * Mat3::Translation(-pivot);
}

auto Transform2D::TransformPoint(const Vec2& point) const -> Vec2 {
    return ToMatrix().TransformPoint(point);
}

} // namespace atom::algo

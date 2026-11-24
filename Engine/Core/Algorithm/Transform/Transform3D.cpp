/**
  * @file           : Transform3D.cpp
  * @author         : Romi Brooks
  * @brief          : Implements 3D transform composition and point mapping.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Transform3D.hpp"

namespace atom::algo {

auto Transform3D::ToMatrix() const -> Mat4 {
    return Mat4::Translation(position) * Mat4::RotationZ(rotation.GetZ()) * Mat4::RotationY(rotation.GetY()) *
           Mat4::RotationX(rotation.GetX()) * Mat4::Scale(scale);
}

auto Transform3D::TransformPoint(const Vec3& point) const -> Vec3 {
    return ToMatrix().TransformPoint(point);
}

} // namespace atom::algo

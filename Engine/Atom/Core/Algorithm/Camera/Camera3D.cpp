/**
  * @file           : Camera3D.cpp
  * @author         : Romi Brooks
  * @brief          : Implements 3D camera view and projection helpers.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Camera3D.hpp"

namespace atom::algo {

auto Camera3D::ViewMatrix() const -> Mat4 {
    return Mat4::LookAt(position, target, up);
}
auto Camera3D::ProjectionMatrix() const -> Mat4 {
    return Mat4::Perspective(vertical_fov, aspect_ratio, near_plane, far_plane);
}
auto Camera3D::ViewProjectionMatrix() const -> Mat4 {
    return ProjectionMatrix() * ViewMatrix();
}

} // namespace atom::algo

/**
  * @file           : Camera3D.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral 3D camera value type.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ALGORITHM_CAMERA3D_HPP
#define ATOM_ALGORITHM_CAMERA3D_HPP

#include <Algorithm/Math/Scalar.hpp>
#include <Algorithm/Matrix/Mat4.hpp>
#include <Algorithm/Vector/Vec3.hpp>

namespace atom::algo {

struct Camera3D {
        Vec3 position = Vec3::Zero();
        Vec3 target = Vec3::UnitZ();
        Vec3 up = Vec3::UnitY();
        float vertical_fov = ToRadians(60.0f);
        float aspect_ratio = 16.0f / 9.0f;
        float near_plane = 0.1f;
        float far_plane = 1000.0f;

        [[nodiscard]] auto ViewMatrix() const -> Mat4;
        [[nodiscard]] auto ProjectionMatrix() const -> Mat4;
        [[nodiscard]] auto ViewProjectionMatrix() const -> Mat4;
};

} // namespace atom::algo

#endif

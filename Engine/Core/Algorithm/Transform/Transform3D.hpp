/**
  * @file           : Transform3D.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral 3D transform value type.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_TRANSFORM3D_HPP
#define ATOM_TRANSFORM3D_HPP

#include <Algorithm/Matrix/Mat4.hpp>
#include <Algorithm/Vector/Vec3.hpp>

namespace atom::algo {

struct Transform3D {
        Vec3 position = Vec3::Zero();
        Vec3 rotation = Vec3::Zero();
        Vec3 scale = Vec3::One();

        [[nodiscard]] auto ToMatrix() const -> Mat4;
        [[nodiscard]] auto TransformPoint(const Vec3& point) const -> Vec3;
};

} // namespace atom::algo

#endif // ATOM_TRANSFORM3D_HPP

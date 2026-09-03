/**
  * @file           : Transform2D.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral 2D transform value type.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_TRANSFORM2D_HPP
#define ATOM_TRANSFORM2D_HPP

#include <Algorithm/Matrix/Mat3.hpp>
#include <Algorithm/Vector/Vec2.hpp>

namespace atom::algo {

struct Transform2D {
        Vec2 position = Vec2::Zero();
        float rotation = 0.0f;
        Vec2 scale = Vec2::One();
        Vec2 pivot = Vec2::Zero();

        [[nodiscard]] auto ToMatrix() const -> Mat3;
        [[nodiscard]] auto TransformPoint(const Vec2& point) const -> Vec2;
};

} // namespace atom::algo

#endif // ATOM_TRANSFORM2D_HPP

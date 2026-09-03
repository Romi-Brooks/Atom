/**
  * @file           : Primitives.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral 2D and 3D primitive value types.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_GEOMETRY_PRIMITIVES_HPP
#define ATOM_GEOMETRY_PRIMITIVES_HPP

#include <algorithm>
#include <cmath>

#include <Algorithm/Vector/Vec2.hpp>
#include <Algorithm/Vector/Vec3.hpp>

namespace atom::algo {

struct Rect2 {
        Vec2 min = Vec2::Zero();
        Vec2 max = Vec2::Zero();

        [[nodiscard]] constexpr auto Size() const -> Vec2 {
            return max - min;
        }
        [[nodiscard]] constexpr auto Center() const -> Vec2 {
            return (min + max) * 0.5f;
        }
        [[nodiscard]] constexpr auto Contains(const Vec2& point) const -> bool {
            return point.GetX() >= min.GetX() && point.GetX() <= max.GetX() && point.GetY() >= min.GetY() &&
                   point.GetY() <= max.GetY();
        }
        [[nodiscard]] constexpr auto Intersects(const Rect2& other) const -> bool {
            return min.GetX() <= other.max.GetX() && max.GetX() >= other.min.GetX() && min.GetY() <= other.max.GetY() &&
                   max.GetY() >= other.min.GetY();
        }
};

struct Circle2 {
        Vec2 center = Vec2::Zero();
        float radius = 0.0f;

        [[nodiscard]] auto Contains(const Vec2& point) const -> bool {
            return Vec2::DistanceSquared(center, point) <= radius * radius;
        }
        [[nodiscard]] auto Intersects(const Circle2& other) const -> bool {
            const float combinedRadius = radius + other.radius;
            return Vec2::DistanceSquared(center, other.center) <= combinedRadius * combinedRadius;
        }
};

struct AABB3 {
        Vec3 min = Vec3::Zero();
        Vec3 max = Vec3::Zero();

        [[nodiscard]] constexpr auto Size() const -> Vec3 {
            return max - min;
        }
        [[nodiscard]] constexpr auto Center() const -> Vec3 {
            return (min + max) * 0.5f;
        }
        [[nodiscard]] constexpr auto Contains(const Vec3& point) const -> bool {
            return point.GetX() >= min.GetX() && point.GetX() <= max.GetX() && point.GetY() >= min.GetY() &&
                   point.GetY() <= max.GetY() && point.GetZ() >= min.GetZ() && point.GetZ() <= max.GetZ();
        }
        [[nodiscard]] constexpr auto Intersects(const AABB3& other) const -> bool {
            return min.GetX() <= other.max.GetX() && max.GetX() >= other.min.GetX() && min.GetY() <= other.max.GetY() &&
                   max.GetY() >= other.min.GetY() && min.GetZ() <= other.max.GetZ() && max.GetZ() >= other.min.GetZ();
        }
};

struct Ray3 {
        Vec3 origin = Vec3::Zero();
        Vec3 direction = Vec3::UnitZ();

        [[nodiscard]] constexpr auto PointAt(const float distance) const -> Vec3 {
            return origin + direction * distance;
        }
};

} // namespace atom::algo

#endif // ATOM_GEOMETRY_PRIMITIVES_HPP

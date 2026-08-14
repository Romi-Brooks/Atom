/**
  * @file           : Vec3.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2026/8/14
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Vec3.hpp"

namespace atom {
auto Vec3::GetX() const -> float {
    return x_;
}

auto Vec3::GetY() const -> float {
    return y_;
}

auto Vec3::GetZ() const -> float {
    return z_;
}

auto Vec3::SetX(const float x) -> void {
    x_ = x;
}

auto Vec3::SetY(const float y) -> void {
    y_ = y;
}

auto Vec3::SetZ(const float z) -> void {
    z_ = z;
}
} // namespace atom

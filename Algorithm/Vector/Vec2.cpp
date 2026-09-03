/**
	* @file           : Vec2.cpp
	* @author         : Romi Brooks
	* @brief          : Vec2 implementation
	* @attention      :
	* @date           : 2026/6/6
	Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// Self Dependencies
#include "Vec2.hpp"

namespace atom::algo {

auto Vec2::GetX() const -> float {
    return x_;
}

auto Vec2::GetY() const -> float {
    return y_;
}

auto Vec2::SetX(const float x) -> void {
    x_ = x;
}

auto Vec2::SetY(const float y) -> void {
    y_ = y;
}

} // namespace atom::algo

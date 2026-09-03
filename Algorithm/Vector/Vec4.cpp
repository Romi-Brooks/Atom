/**
  * @file           : Vec4.cpp
  * @author         : Romi Brooks
  * @brief          : Implements four-component vector helpers.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Vec4.hpp"

namespace atom::algo {

auto Vec4::Normalized(const float epsilon) const -> Vec4 {
    const float length = Length();
    return length <= epsilon ? Zero() : *this / length;
}

} // namespace atom::algo

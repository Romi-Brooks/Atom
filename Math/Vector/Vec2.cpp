/**
* @file             : Vec2.cpp
  * @author         : Romi Brooks
  * @brief          : Vector 2d for Atom Engine
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_VEC2_CPP
#define ATOM_VEC2_CPP
#include "Vec2.hpp"

auto atom::Vec2::GetX() const -> int {
    return x_;
}

auto atom::Vec2::GetY() const -> int {
    return y_;
}

auto atom::Vec2::SetX(const int x) {
    this->x_ = x;
}

auto atom::Vec2::SetY(const int y) {
    this->y_ = y;
}

#endif // ATOM_VEC2_CPP

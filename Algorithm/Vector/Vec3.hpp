/**
  * @file           : Vec3.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2026/8/14
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_VEC3_H
#define ATOM_VEC3_H

namespace atom {
    class Vec3 {
        private:
            float x_ = 0;
            float y_ = 0;
            float z_ = 0;

        public:
            Vec3() = default;
            Vec3(const float x, const float y, const float z) : x_(x), y_(y), z_(z) {}

            [[nodiscard]] auto GetX() const -> float;
            [[nodiscard]] auto GetY() const -> float;
            [[nodiscard]] auto GetZ() const -> float;

            auto SetX(float x) -> void;
            auto SetY(float y) -> void;
            auto SetZ(float z) -> void;
    };
}

#endif //ATOM_VEC3_H

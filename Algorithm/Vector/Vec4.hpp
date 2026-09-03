/**
  * @file           : Vec4.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral four-component vector type.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ALGORITHM_VEC4_HPP
#define ATOM_ALGORITHM_VEC4_HPP

#include <cmath>

namespace atom::algo {

class Vec4 {
    private:
        float x_ = 0.0f;
        float y_ = 0.0f;
        float z_ = 0.0f;
        float w_ = 0.0f;

    public:
        constexpr Vec4() = default;
        constexpr Vec4(float x, float y, float z, float w) : x_(x), y_(y), z_(z), w_(w) {}

        [[nodiscard]] constexpr auto GetX() const -> float {
            return x_;
        }
        [[nodiscard]] constexpr auto GetY() const -> float {
            return y_;
        }
        [[nodiscard]] constexpr auto GetZ() const -> float {
            return z_;
        }
        [[nodiscard]] constexpr auto GetW() const -> float {
            return w_;
        }
        constexpr auto SetX(float x) -> void {
            x_ = x;
        }
        constexpr auto SetY(float y) -> void {
            y_ = y;
        }
        constexpr auto SetZ(float z) -> void {
            z_ = z;
        }
        constexpr auto SetW(float w) -> void {
            w_ = w;
        }

        [[nodiscard]] constexpr auto LengthSquared() const -> float {
            return x_ * x_ + y_ * y_ + z_ * z_ + w_ * w_;
        }
        [[nodiscard]] auto Length() const -> float {
            return std::sqrt(LengthSquared());
        }
        [[nodiscard]] auto Normalized(float epsilon = 1.0e-6f) const -> Vec4;
        [[nodiscard]] constexpr auto Dot(const Vec4& other) const -> float {
            return x_ * other.x_ + y_ * other.y_ + z_ * other.z_ + w_ * other.w_;
        }

        [[nodiscard]] static constexpr auto Zero() -> Vec4 {
            return {};
        }
        [[nodiscard]] static constexpr auto One() -> Vec4 {
            return {1.0f, 1.0f, 1.0f, 1.0f};
        }
        [[nodiscard]] static constexpr auto Lerp(const Vec4& from, const Vec4& to, float amount) -> Vec4 {
            return from + (to - from) * amount;
        }

        [[nodiscard]] constexpr auto operator-() const -> Vec4 {
            return {-x_, -y_, -z_, -w_};
        }
        [[nodiscard]] constexpr auto operator+(const Vec4& other) const -> Vec4 {
            return {x_ + other.x_, y_ + other.y_, z_ + other.z_, w_ + other.w_};
        }
        [[nodiscard]] constexpr auto operator-(const Vec4& other) const -> Vec4 {
            return {x_ - other.x_, y_ - other.y_, z_ - other.z_, w_ - other.w_};
        }
        [[nodiscard]] constexpr auto operator*(float scalar) const -> Vec4 {
            return {x_ * scalar, y_ * scalar, z_ * scalar, w_ * scalar};
        }
        [[nodiscard]] constexpr auto operator/(float scalar) const -> Vec4 {
            return {x_ / scalar, y_ / scalar, z_ / scalar, w_ / scalar};
        }
        constexpr auto operator+=(const Vec4& other) -> Vec4& {
            return *this = *this + other;
        }
        constexpr auto operator-=(const Vec4& other) -> Vec4& {
            return *this = *this - other;
        }
        constexpr auto operator*=(float scalar) -> Vec4& {
            return *this = *this * scalar;
        }
        constexpr auto operator/=(float scalar) -> Vec4& {
            return *this = *this / scalar;
        }
        [[nodiscard]] constexpr auto operator==(const Vec4&) const -> bool = default;
};

[[nodiscard]] constexpr auto operator*(float scalar, const Vec4& value) -> Vec4 {
    return value * scalar;
}

} // namespace atom::algo

#endif // ATOM_ALGORITHM_VEC4_HPP

#ifndef ATOM_ALGORITHM_VEC2_HPP
#define ATOM_ALGORITHM_VEC2_HPP

#include <cmath>

namespace atom::algo {

class Vec2 {
    private:
        float x_ = 0.0f;
        float y_ = 0.0f;

    public:
        constexpr Vec2() = default;
        constexpr Vec2(float x, float y) : x_(x), y_(y) {}

        [[nodiscard]] constexpr auto GetX() const -> float {
            return x_;
        }
        [[nodiscard]] constexpr auto GetY() const -> float {
            return y_;
        }
        constexpr auto SetX(float x) -> void {
            x_ = x;
        }
        constexpr auto SetY(float y) -> void {
            y_ = y;
        }

        [[nodiscard]] constexpr auto LengthSquared() const -> float {
            return x_ * x_ + y_ * y_;
        }
        [[nodiscard]] auto Length() const -> float {
            return std::sqrt(LengthSquared());
        }
        [[nodiscard]] auto Normalized(float epsilon = 1.0e-6f) const -> Vec2;
        auto Normalize(float epsilon = 1.0e-6f) -> bool;

        [[nodiscard]] constexpr auto Dot(const Vec2& other) const -> float {
            return x_ * other.x_ + y_ * other.y_;
        }
        [[nodiscard]] constexpr auto Cross(const Vec2& other) const -> float {
            return x_ * other.y_ - y_ * other.x_;
        }
        [[nodiscard]] auto Rotated(float radians) const -> Vec2;
        [[nodiscard]] auto Reflected(const Vec2& normal) const -> Vec2;

        [[nodiscard]] static constexpr auto Zero() -> Vec2 {
            return {};
        }
        [[nodiscard]] static constexpr auto One() -> Vec2 {
            return {1.0f, 1.0f};
        }
        [[nodiscard]] static constexpr auto UnitX() -> Vec2 {
            return {1.0f, 0.0f};
        }
        [[nodiscard]] static constexpr auto UnitY() -> Vec2 {
            return {0.0f, 1.0f};
        }
        [[nodiscard]] static constexpr auto Lerp(const Vec2& from, const Vec2& to, float amount) -> Vec2 {
            return from + (to - from) * amount;
        }
        [[nodiscard]] static auto Distance(const Vec2& from, const Vec2& to) -> float {
            return (to - from).Length();
        }
        [[nodiscard]] static constexpr auto DistanceSquared(const Vec2& from, const Vec2& to) -> float {
            return (to - from).LengthSquared();
        }

        [[nodiscard]] constexpr auto operator+() const -> Vec2 {
            return *this;
        }
        [[nodiscard]] constexpr auto operator-() const -> Vec2 {
            return {-x_, -y_};
        }
        [[nodiscard]] constexpr auto operator+(const Vec2& other) const -> Vec2 {
            return {x_ + other.x_, y_ + other.y_};
        }
        [[nodiscard]] constexpr auto operator-(const Vec2& other) const -> Vec2 {
            return {x_ - other.x_, y_ - other.y_};
        }
        [[nodiscard]] constexpr auto operator*(float scalar) const -> Vec2 {
            return {x_ * scalar, y_ * scalar};
        }
        [[nodiscard]] constexpr auto operator/(float scalar) const -> Vec2 {
            return {x_ / scalar, y_ / scalar};
        }
        constexpr auto operator+=(const Vec2& other) -> Vec2& {
            return *this = *this + other;
        }
        constexpr auto operator-=(const Vec2& other) -> Vec2& {
            return *this = *this - other;
        }
        constexpr auto operator*=(float scalar) -> Vec2& {
            return *this = *this * scalar;
        }
        constexpr auto operator/=(float scalar) -> Vec2& {
            return *this = *this / scalar;
        }
        [[nodiscard]] constexpr auto operator==(const Vec2&) const -> bool = default;
};

[[nodiscard]] constexpr auto operator*(float scalar, const Vec2& value) -> Vec2 {
    return value * scalar;
}

} // namespace atom::algo

#endif // ATOM_ALGORITHM_VEC2_HPP

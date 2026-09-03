#ifndef ATOM_ALGORITHM_VEC3_HPP
#define ATOM_ALGORITHM_VEC3_HPP

#include <cmath>

namespace atom::algo {

class Vec3 {
    private:
        float x_ = 0.0f;
        float y_ = 0.0f;
        float z_ = 0.0f;

    public:
        constexpr Vec3() = default;
        constexpr Vec3(float x, float y, float z) : x_(x), y_(y), z_(z) {}

        [[nodiscard]] constexpr auto GetX() const -> float {
            return x_;
        }
        [[nodiscard]] constexpr auto GetY() const -> float {
            return y_;
        }
        [[nodiscard]] constexpr auto GetZ() const -> float {
            return z_;
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

        [[nodiscard]] constexpr auto LengthSquared() const -> float {
            return x_ * x_ + y_ * y_ + z_ * z_;
        }
        [[nodiscard]] auto Length() const -> float {
            return std::sqrt(LengthSquared());
        }
        [[nodiscard]] auto Normalized(float epsilon = 1.0e-6f) const -> Vec3;
        auto Normalize(float epsilon = 1.0e-6f) -> bool;
        [[nodiscard]] constexpr auto Dot(const Vec3& other) const -> float {
            return x_ * other.x_ + y_ * other.y_ + z_ * other.z_;
        }
        [[nodiscard]] constexpr auto Cross(const Vec3& other) const -> Vec3 {
            return {y_ * other.z_ - z_ * other.y_, z_ * other.x_ - x_ * other.z_, x_ * other.y_ - y_ * other.x_};
        }

        [[nodiscard]] static constexpr auto Zero() -> Vec3 {
            return {};
        }
        [[nodiscard]] static constexpr auto One() -> Vec3 {
            return {1.0f, 1.0f, 1.0f};
        }
        [[nodiscard]] static constexpr auto UnitX() -> Vec3 {
            return {1.0f, 0.0f, 0.0f};
        }
        [[nodiscard]] static constexpr auto UnitY() -> Vec3 {
            return {0.0f, 1.0f, 0.0f};
        }
        [[nodiscard]] static constexpr auto UnitZ() -> Vec3 {
            return {0.0f, 0.0f, 1.0f};
        }
        [[nodiscard]] static constexpr auto Lerp(const Vec3& from, const Vec3& to, float amount) -> Vec3 {
            return from + (to - from) * amount;
        }
        [[nodiscard]] static auto Distance(const Vec3& from, const Vec3& to) -> float {
            return (to - from).Length();
        }
        [[nodiscard]] static constexpr auto DistanceSquared(const Vec3& from, const Vec3& to) -> float {
            return (to - from).LengthSquared();
        }

        [[nodiscard]] constexpr auto operator+() const -> Vec3 {
            return *this;
        }
        [[nodiscard]] constexpr auto operator-() const -> Vec3 {
            return {-x_, -y_, -z_};
        }
        [[nodiscard]] constexpr auto operator+(const Vec3& other) const -> Vec3 {
            return {x_ + other.x_, y_ + other.y_, z_ + other.z_};
        }
        [[nodiscard]] constexpr auto operator-(const Vec3& other) const -> Vec3 {
            return {x_ - other.x_, y_ - other.y_, z_ - other.z_};
        }
        [[nodiscard]] constexpr auto operator*(float scalar) const -> Vec3 {
            return {x_ * scalar, y_ * scalar, z_ * scalar};
        }
        [[nodiscard]] constexpr auto operator/(float scalar) const -> Vec3 {
            return {x_ / scalar, y_ / scalar, z_ / scalar};
        }
        constexpr auto operator+=(const Vec3& other) -> Vec3& {
            return *this = *this + other;
        }
        constexpr auto operator-=(const Vec3& other) -> Vec3& {
            return *this = *this - other;
        }
        constexpr auto operator*=(float scalar) -> Vec3& {
            return *this = *this * scalar;
        }
        constexpr auto operator/=(float scalar) -> Vec3& {
            return *this = *this / scalar;
        }
        [[nodiscard]] constexpr auto operator==(const Vec3&) const -> bool = default;
};

[[nodiscard]] constexpr auto operator*(float scalar, const Vec3& value) -> Vec3 {
    return value * scalar;
}

} // namespace atom::algo

#endif // ATOM_ALGORITHM_VEC3_HPP

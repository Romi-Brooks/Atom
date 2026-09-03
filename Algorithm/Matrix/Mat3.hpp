/**
  * @file           : Mat3.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral column-major 3x3 matrix type.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ALGORITHM_MAT3_HPP
#define ATOM_ALGORITHM_MAT3_HPP

#include <array>
#include <optional>

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::algo {

// Column-major 3x3 matrix operating on column vectors.
class Mat3 {
    private:
        std::array<float, 9> elements_{};

    public:
        constexpr Mat3() = default;
        explicit constexpr Mat3(std::array<float, 9> elements) : elements_(elements) {}

        [[nodiscard]] static constexpr auto Identity() -> Mat3 {
            return Mat3{{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
        }
        [[nodiscard]] static auto Translation(const Vec2& translation) -> Mat3;
        [[nodiscard]] static auto Rotation(float radians) -> Mat3;
        [[nodiscard]] static auto Scale(const Vec2& scale) -> Mat3;

        [[nodiscard]] constexpr auto operator()(std::size_t row, std::size_t column) const -> float {
            return elements_[column * 3 + row];
        }
        constexpr auto operator()(std::size_t row, std::size_t column) -> float& {
            return elements_[column * 3 + row];
        }
        [[nodiscard]] constexpr auto Data() const -> const float* {
            return elements_.data();
        }

        [[nodiscard]] auto operator*(const Mat3& other) const -> Mat3;
        [[nodiscard]] auto TransformPoint(const Vec2& point) const -> Vec2;
        [[nodiscard]] auto TransformVector(const Vec2& vector) const -> Vec2;
        [[nodiscard]] auto Determinant() const -> float;
        [[nodiscard]] auto Inverse(float epsilon = 1.0e-6f) const -> std::optional<Mat3>;
};

} // namespace atom::algo

#endif // ATOM_ALGORITHM_MAT3_HPP

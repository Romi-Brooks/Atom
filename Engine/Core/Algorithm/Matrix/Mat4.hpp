/**
  * @file           : Mat4.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral column-major 4x4 matrix type.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ALGORITHM_MAT4_HPP
#define ATOM_ALGORITHM_MAT4_HPP

#include <array>

#include <Algorithm/Vector/Vec3.hpp>
#include <Algorithm/Vector/Vec4.hpp>

namespace atom::algo {

// Column-major matrix for column vectors. Projection helpers use SDL_GPU's
// left-handed coordinate system and [0, 1] normalized device depth.
class Mat4 {
    private:
        std::array<float, 16> elements_{};

    public:
        constexpr Mat4() = default;
        explicit constexpr Mat4(std::array<float, 16> elements) : elements_(elements) {}

        [[nodiscard]] static constexpr auto Identity() -> Mat4 {
            return Mat4{
                {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
        }
        [[nodiscard]] static auto Translation(const Vec3& translation) -> Mat4;
        [[nodiscard]] static auto Scale(const Vec3& scale) -> Mat4;
        [[nodiscard]] static auto RotationX(float radians) -> Mat4;
        [[nodiscard]] static auto RotationY(float radians) -> Mat4;
        [[nodiscard]] static auto RotationZ(float radians) -> Mat4;
        [[nodiscard]] static auto Perspective(float vertical_fov_radians, float aspect_ratio, float near_plane,
                                              float far_plane) -> Mat4;
        [[nodiscard]] static auto Orthographic(float left, float right, float bottom, float top, float near_plane,
                                               float far_plane) -> Mat4;
        [[nodiscard]] static auto LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) -> Mat4;

        [[nodiscard]] constexpr auto operator()(std::size_t row, std::size_t column) const -> float {
            return elements_[column * 4 + row];
        }
        constexpr auto operator()(std::size_t row, std::size_t column) -> float& {
            return elements_[column * 4 + row];
        }
        [[nodiscard]] constexpr auto Data() const -> const float* {
            return elements_.data();
        }

        [[nodiscard]] auto operator*(const Mat4& other) const -> Mat4;
        [[nodiscard]] auto operator*(const Vec4& vector) const -> Vec4;
        [[nodiscard]] auto TransformPoint(const Vec3& point) const -> Vec3;
        [[nodiscard]] auto TransformVector(const Vec3& vector) const -> Vec3;
};

} // namespace atom::algo

#endif // ATOM_ALGORITHM_MAT4_HPP

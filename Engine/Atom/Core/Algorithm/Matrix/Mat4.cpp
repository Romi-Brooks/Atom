/**
  * @file           : Mat4.cpp
  * @author         : Romi Brooks
  * @brief          : Implements column-major 4x4 matrix operations.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Mat4.hpp"

#include <cmath>
#include <stdexcept>

namespace atom::algo {

auto Mat4::Translation(const Vec3& translation) -> Mat4 {
    Mat4 result = Identity();
    result(0, 3) = translation.GetX();
    result(1, 3) = translation.GetY();
    result(2, 3) = translation.GetZ();
    return result;
}

auto Mat4::Scale(const Vec3& scale) -> Mat4 {
    Mat4 result = Identity();
    result(0, 0) = scale.GetX();
    result(1, 1) = scale.GetY();
    result(2, 2) = scale.GetZ();
    return result;
}

auto Mat4::RotationX(const float radians) -> Mat4 {
    Mat4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result(1, 1) = cosine;
    result(1, 2) = -sine;
    result(2, 1) = sine;
    result(2, 2) = cosine;
    return result;
}

auto Mat4::RotationY(const float radians) -> Mat4 {
    Mat4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result(0, 0) = cosine;
    result(0, 2) = sine;
    result(2, 0) = -sine;
    result(2, 2) = cosine;
    return result;
}

auto Mat4::RotationZ(const float radians) -> Mat4 {
    Mat4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result(0, 0) = cosine;
    result(0, 1) = -sine;
    result(1, 0) = sine;
    result(1, 1) = cosine;
    return result;
}

auto Mat4::Perspective(const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
                       const float far_plane) -> Mat4 {
    if (aspect_ratio <= 0.0f || near_plane <= 0.0f || far_plane <= near_plane)
        throw std::invalid_argument("Invalid perspective projection parameters");
    const float y_scale = 1.0f / std::tan(vertical_fov_radians * 0.5f);
    Mat4 result;
    result(0, 0) = y_scale / aspect_ratio;
    result(1, 1) = y_scale;
    result(2, 2) = far_plane / (far_plane - near_plane);
    result(2, 3) = -(near_plane * far_plane) / (far_plane - near_plane);
    result(3, 2) = 1.0f;
    return result;
}

auto Mat4::Orthographic(const float left, const float right, const float bottom, const float top,
                        const float near_plane, const float far_plane) -> Mat4 {
    if (right == left || top == bottom || far_plane == near_plane)
        throw std::invalid_argument("Invalid orthographic projection parameters");
    Mat4 result = Identity();
    result(0, 0) = 2.0f / (right - left);
    result(1, 1) = 2.0f / (top - bottom);
    result(2, 2) = 1.0f / (far_plane - near_plane);
    result(0, 3) = -(right + left) / (right - left);
    result(1, 3) = -(top + bottom) / (top - bottom);
    result(2, 3) = -near_plane / (far_plane - near_plane);
    return result;
}

auto Mat4::LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) -> Mat4 {
    const Vec3 forward = (target - eye).Normalized();
    const Vec3 right = up.Cross(forward).Normalized();
    const Vec3 camera_up = forward.Cross(right);
    Mat4 result = Identity();
    result(0, 0) = right.GetX();
    result(0, 1) = right.GetY();
    result(0, 2) = right.GetZ();
    result(0, 3) = -right.Dot(eye);
    result(1, 0) = camera_up.GetX();
    result(1, 1) = camera_up.GetY();
    result(1, 2) = camera_up.GetZ();
    result(1, 3) = -camera_up.Dot(eye);
    result(2, 0) = forward.GetX();
    result(2, 1) = forward.GetY();
    result(2, 2) = forward.GetZ();
    result(2, 3) = -forward.Dot(eye);
    return result;
}

auto Mat4::operator*(const Mat4& other) const -> Mat4 {
    Mat4 result;
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t index = 0; index < 4; ++index)
                result(row, column) += (*this)(row, index) * other(index, column);
    return result;
}

auto Mat4::operator*(const Vec4& vector) const -> Vec4 {
    return {(*this)(0, 0) * vector.GetX() + (*this)(0, 1) * vector.GetY() + (*this)(0, 2) * vector.GetZ() +
                (*this)(0, 3) * vector.GetW(),
            (*this)(1, 0) * vector.GetX() + (*this)(1, 1) * vector.GetY() + (*this)(1, 2) * vector.GetZ() +
                (*this)(1, 3) * vector.GetW(),
            (*this)(2, 0) * vector.GetX() + (*this)(2, 1) * vector.GetY() + (*this)(2, 2) * vector.GetZ() +
                (*this)(2, 3) * vector.GetW(),
            (*this)(3, 0) * vector.GetX() + (*this)(3, 1) * vector.GetY() + (*this)(3, 2) * vector.GetZ() +
                (*this)(3, 3) * vector.GetW()};
}

auto Mat4::TransformPoint(const Vec3& point) const -> Vec3 {
    const Vec4 transformed = *this * Vec4{point.GetX(), point.GetY(), point.GetZ(), 1.0f};
    const float inverse_w = transformed.GetW() == 0.0f ? 1.0f : 1.0f / transformed.GetW();
    return {transformed.GetX() * inverse_w, transformed.GetY() * inverse_w, transformed.GetZ() * inverse_w};
}

auto Mat4::TransformVector(const Vec3& vector) const -> Vec3 {
    const Vec4 transformed = *this * Vec4{vector.GetX(), vector.GetY(), vector.GetZ(), 0.0f};
    return {transformed.GetX(), transformed.GetY(), transformed.GetZ()};
}

} // namespace atom::algo

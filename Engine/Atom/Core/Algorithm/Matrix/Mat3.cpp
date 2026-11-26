/**
  * @file           : Mat3.cpp
  * @author         : Romi Brooks
  * @brief          : Implements column-major 3x3 matrix operations.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Mat3.hpp"

#include <cmath>

namespace atom::algo {

auto Mat3::Translation(const Vec2& translation) -> Mat3 {
    Mat3 result = Identity();
    result(0, 2) = translation.GetX();
    result(1, 2) = translation.GetY();
    return result;
}

auto Mat3::Rotation(const float radians) -> Mat3 {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    Mat3 result = Identity();
    result(0, 0) = cosine;
    result(0, 1) = -sine;
    result(1, 0) = sine;
    result(1, 1) = cosine;
    return result;
}

auto Mat3::Scale(const Vec2& scale) -> Mat3 {
    Mat3 result = Identity();
    result(0, 0) = scale.GetX();
    result(1, 1) = scale.GetY();
    return result;
}

auto Mat3::operator*(const Mat3& other) const -> Mat3 {
    Mat3 result;
    for (std::size_t column = 0; column < 3; ++column)
        for (std::size_t row = 0; row < 3; ++row)
            for (std::size_t index = 0; index < 3; ++index)
                result(row, column) += (*this)(row, index) * other(index, column);
    return result;
}

auto Mat3::TransformPoint(const Vec2& point) const -> Vec2 {
    return {(*this)(0, 0) * point.GetX() + (*this)(0, 1) * point.GetY() + (*this)(0, 2),
            (*this)(1, 0) * point.GetX() + (*this)(1, 1) * point.GetY() + (*this)(1, 2)};
}

auto Mat3::TransformVector(const Vec2& vector) const -> Vec2 {
    return {(*this)(0, 0) * vector.GetX() + (*this)(0, 1) * vector.GetY(),
            (*this)(1, 0) * vector.GetX() + (*this)(1, 1) * vector.GetY()};
}

auto Mat3::Determinant() const -> float {
    return (*this)(0, 0) * ((*this)(1, 1) * (*this)(2, 2) - (*this)(1, 2) * (*this)(2, 1)) -
           (*this)(0, 1) * ((*this)(1, 0) * (*this)(2, 2) - (*this)(1, 2) * (*this)(2, 0)) +
           (*this)(0, 2) * ((*this)(1, 0) * (*this)(2, 1) - (*this)(1, 1) * (*this)(2, 0));
}

auto Mat3::Inverse(const float epsilon) const -> std::optional<Mat3> {
    const float determinant = Determinant();
    if (std::abs(determinant) <= epsilon)
        return std::nullopt;

    Mat3 result;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            const std::size_t r0 = (column + 1) % 3;
            const std::size_t r1 = (column + 2) % 3;
            const std::size_t c0 = (row + 1) % 3;
            const std::size_t c1 = (row + 2) % 3;
            result(row, column) = ((*this)(r0, c0) * (*this)(r1, c1) - (*this)(r0, c1) * (*this)(r1, c0)) / determinant;
        }
    }
    return result;
}

} // namespace atom::algo

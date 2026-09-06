/**
 * @file           : ColorMath.hpp
 * @brief          : Stateless operations on shared color values.
 */

#ifndef ATOM_COLOR_COLOR_MATH_HPP
#define ATOM_COLOR_COLOR_MATH_HPP

#include <Algorithm/Math/Scalar.hpp>
#include <Color/Color.hpp>

namespace atom::color {

[[nodiscard]] constexpr auto ApplyOpacity(Color color, const float opacity) -> Color {
    color.a = static_cast<uint8_t>(static_cast<float>(color.a) * algo::Saturate(opacity));
    return color;
}

[[nodiscard]] constexpr auto Blend(const Color left, const Color right, const float amount) -> Color {
    const auto t = algo::Saturate(amount);
    return {.r = static_cast<uint8_t>(left.r + (right.r - left.r) * t),
            .g = static_cast<uint8_t>(left.g + (right.g - left.g) * t),
            .b = static_cast<uint8_t>(left.b + (right.b - left.b) * t),
            .a = static_cast<uint8_t>(left.a + (right.a - left.a) * t)};
}

} // namespace atom::color

#endif // ATOM_COLOR_COLOR_MATH_HPP

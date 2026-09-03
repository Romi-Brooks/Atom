/**
  * @file           : Scalar.hpp
  * @author         : Romi Brooks
  * @brief          : Shared scalar constants and numeric helpers.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ALGORITHM_SCALAR_HPP
#define ATOM_ALGORITHM_SCALAR_HPP

#include <algorithm>
#include <cmath>
#include <numbers>

namespace atom::algo {

inline constexpr float Pi = std::numbers::pi_v<float>;

[[nodiscard]] constexpr auto ToRadians(float degrees) -> float {
    return degrees * Pi / 180.0f;
}
[[nodiscard]] constexpr auto ToDegrees(float radians) -> float {
    return radians * 180.0f / Pi;
}
[[nodiscard]] constexpr auto Clamp(float value, float minimum, float maximum) -> float {
    return std::clamp(value, minimum, maximum);
}
[[nodiscard]] constexpr auto Lerp(float from, float to, float amount) -> float {
    return from + (to - from) * amount;
}
[[nodiscard]] constexpr auto InverseLerp(float from, float to, float value) -> float {
    return from == to ? 0.0f : (value - from) / (to - from);
}
[[nodiscard]] constexpr auto SmoothStep(float from, float to, float value) -> float {
    const float amount = Clamp(InverseLerp(from, to, value), 0.0f, 1.0f);
    return amount * amount * (3.0f - 2.0f * amount);
}
[[nodiscard]] inline auto NearlyEqual(float left, float right, float epsilon = 1.0e-5f) -> bool {
    return std::abs(left - right) <= epsilon * std::max({1.0f, std::abs(left), std::abs(right)});
}

} // namespace atom::algo

#endif // ATOM_ALGORITHM_SCALAR_HPP

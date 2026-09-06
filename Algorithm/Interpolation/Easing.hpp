/**
 * @file           : Easing.hpp
 * @brief          : Pure normalized interpolation curves.
 */

#ifndef ATOM_ALGORITHM_INTERPOLATION_EASING_HPP
#define ATOM_ALGORITHM_INTERPOLATION_EASING_HPP

#include <Algorithm/Math/Scalar.hpp>

namespace atom::algo::easing {

[[nodiscard]] constexpr auto InCubic(const float value) -> float {
    const auto clamped = Saturate(value);
    return clamped * clamped * clamped;
}

[[nodiscard]] constexpr auto OutCubic(const float value) -> float {
    const auto inverse = 1.0f - Saturate(value);
    return 1.0f - inverse * inverse * inverse;
}

[[nodiscard]] constexpr auto OutBack(const float value, const float overshoot = 1.70158f) -> float {
    const auto shifted = Saturate(value) - 1.0f;
    return 1.0f + (overshoot + 1.0f) * shifted * shifted * shifted + overshoot * shifted * shifted;
}

// These curves form an equal-power fade pair: OutSine(t)^2 +
// (1 - InSine(t))^2 equals one for t in [0, 1].
[[nodiscard]] inline auto InSine(const float value) -> float {
    return 1.0f - std::cos(Saturate(value) * Pi * 0.5f);
}

[[nodiscard]] inline auto OutSine(const float value) -> float {
    return std::sin(Saturate(value) * Pi * 0.5f);
}

// Maps a segment of a parent timeline to [0, 1]. Invalid or empty intervals
// resolve to 0 rather than introducing a division-by-zero into interpolation code.
[[nodiscard]] constexpr auto IntervalProgress(const float progress, const float start, const float end) -> float {
    return end <= start ? 0.0f : Saturate((progress - start) / (end - start));
}

} // namespace atom::algo::easing

#endif // ATOM_ALGORITHM_INTERPOLATION_EASING_HPP

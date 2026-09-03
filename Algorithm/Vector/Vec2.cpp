#include "Vec2.hpp"

namespace atom::algo {

auto Vec2::Normalized(const float epsilon) const -> Vec2 {
    const float length = Length();
    return length <= epsilon ? Zero() : *this / length;
}

auto Vec2::Normalize(const float epsilon) -> bool {
    const float length = Length();
    if (length <= epsilon)
        return false;
    *this /= length;
    return true;
}

auto Vec2::Rotated(const float radians) const -> Vec2 {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {x_ * cosine - y_ * sine, x_ * sine + y_ * cosine};
}

auto Vec2::Reflected(const Vec2& normal) const -> Vec2 {
    const Vec2 unit_normal = normal.Normalized();
    return *this - 2.0f * Dot(unit_normal) * unit_normal;
}

} // namespace atom::algo

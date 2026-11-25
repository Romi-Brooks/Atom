#include "Vec3.hpp"

namespace atom::algo {

auto Vec3::Normalized(const float epsilon) const -> Vec3 {
    const float length = Length();
    return length <= epsilon ? Zero() : *this / length;
}

auto Vec3::Normalize(const float epsilon) -> bool {
    const float length = Length();
    if (length <= epsilon)
        return false;
    *this /= length;
    return true;
}

} // namespace atom::algo

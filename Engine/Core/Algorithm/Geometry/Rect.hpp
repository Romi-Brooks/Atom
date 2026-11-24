/**
 * @file           : Rect.hpp
 * @brief          : Shared axis-aligned two-dimensional rectangle value type.
**/

#ifndef ATOM_ALGORITHM_GEOMETRY_RECT_HPP
#define ATOM_ALGORITHM_GEOMETRY_RECT_HPP

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::algo {

struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        [[nodiscard]] constexpr auto Contains(const float point_x, const float point_y) const -> bool {
            return point_x >= x && point_x <= x + width && point_y >= y && point_y <= y + height;
        }

        [[nodiscard]] constexpr auto Contains(const Vec2& point) const -> bool {
            return Contains(point.GetX(), point.GetY());
        }

        [[nodiscard]] constexpr auto Translated(const float offset_x, const float offset_y) const -> Rect {
            return {x + offset_x, y + offset_y, width, height};
        }

        [[nodiscard]] constexpr auto ScaledAboutCenter(const float scale) const -> Rect {
            const auto scaled_width = width * scale;
            const auto scaled_height = height * scale;
            return {x + (width - scaled_width) * 0.5f, y + (height - scaled_height) * 0.5f, scaled_width,
                    scaled_height};
        }
};

} // namespace atom::algo

#endif // ATOM_ALGORITHM_GEOMETRY_RECT_HPP

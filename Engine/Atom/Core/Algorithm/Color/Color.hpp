/**
 * @file           : Color.hpp
 * @brief          : Shared straight-alpha RGBA8 color value.
 */

#ifndef ATOM_COLOR_COLOR_HPP
#define ATOM_COLOR_COLOR_HPP

#include <cstdint>

namespace atom::color {

struct Color {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 255;

        [[nodiscard]] static constexpr auto Black() -> Color { return {.r = 0, .g = 0, .b = 0}; }
        [[nodiscard]] static constexpr auto White() -> Color { return {.r = 255, .g = 255, .b = 255}; }
        [[nodiscard]] static constexpr auto Red() -> Color { return {.r = 255, .g = 0, .b = 0}; }
        [[nodiscard]] static constexpr auto Green() -> Color { return {.r = 0, .g = 255, .b = 0}; }
        [[nodiscard]] static constexpr auto Blue() -> Color { return {.r = 0, .g = 0, .b = 255}; }
};

} // namespace atom::color

#endif // ATOM_COLOR_COLOR_HPP

/**
  * @file           : RenderTypes.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral render color and resource value types.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_RENDERTYPES_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_RENDERTYPES_HPP

#include <cstdint>

namespace atom::render {

struct Color {
        uint8_t r = 0, g = 0, b = 0, a = 255;

        static constexpr Color Black() { return {0, 0, 0}; }
        static constexpr Color White() { return {255, 255, 255}; }
        static constexpr Color Red() { return {255, 0, 0}; }
        static constexpr Color Green() { return {0, 255, 0}; }
        static constexpr Color Blue() { return {0, 0, 255}; }
};

struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
};

} // namespace atom::render

#endif // ATOM_BACKEND_CONTRACTS_RENDER_RENDERTYPES_HPP

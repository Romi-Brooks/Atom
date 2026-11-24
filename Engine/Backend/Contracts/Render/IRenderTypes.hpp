/**
  * @file           : RenderTypes.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral render color and resource value types.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_IRENDERTYPES_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_IRENDERTYPES_HPP

#include <Algorithm/Color/Color.hpp>

namespace atom::render {

using Color = color::Color;

// Render-record rectangle. This is deliberately separate from algo::Rect:
// its coordinates may represent backend-facing world, texel, or framebuffer
// data, while the public Renderer2D API accepts algo::Rect and converts it.
struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
};

} // namespace atom::render

#endif // ATOM_BACKEND_CONTRACTS_RENDER_IRENDERTYPES_HPP

/**
 * @file           : GeometryConversions.hpp
 * @brief          : Explicit adapters between geometry values and render values.
 */

#ifndef ATOM_RENDER_CONVERSION_GEOMETRY_CONVERSIONS_HPP
#define ATOM_RENDER_CONVERSION_GEOMETRY_CONVERSIONS_HPP

#include <Algorithm/Geometry/Rect.hpp>
#include <Backend/Contracts/Render/IRenderTypes.hpp>

namespace atom::render {

/**
 * @brief 将不可渲染的几何Rect转换为可渲染的Rect类型
 * @param rect 是一个几何层的Rect类型
 * @return 一个可以被Render2D渲染的Rect类型
 */
[[nodiscard]] constexpr auto ToRect(const algo::Rect rect) -> Rect {
    return {rect.x, rect.y, rect.width, rect.height};
}

} // namespace atom::render

#endif // ATOM_RENDER_CONVERSION_GEOMETRY_CONVERSIONS_HPP

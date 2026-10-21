// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Shape.hpp
 * @brief 碰撞形状的值类型 + 几何/质量属性的纯函数。
 *
 * 形状是纯数据，定义在刚体的「局部空间」（原点 = 刚体原点，旋转 = 刚体角度）。
 * 碰撞算法（窄相位）是接收两个 Shape 的纯函数，因此新增形状类型只需：
 *   1. 在 Kind 加一个枚举值 + 一个数据字段；
 *   2. 加对应的几何函数分支；
 *   3. 加对应的窄相位函数对（M3 阶段）。
 * 不改动 Shape 的既有成员，也不改动求解器。
 *
 * 凸多边形按逆时针（CCW）存储，顶点数上限 8（对齐 Box2D）。非凸多边形由
 * MakePolygon 拒绝（返回 PhysicsNonConvexPolygon）；复合形状（多个凸形状拼成
 * 一个刚体）是后续里程碑的扩展点。
 */

#ifndef ATOM_PHYSICS_SHAPES_SHAPE_HPP
#define ATOM_PHYSICS_SHAPES_SHAPE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <Algorithm/Geometry/Primitives.hpp>
#include <Algorithm/Vector/Vec2.hpp>
#include <Error/Error.hpp>
#include <Physics/Shapes/MassProperties.hpp>

namespace atom::physics {

struct Circle final {
        algo::Vec2 center{}; // 局部空间圆心（相对刚体原点的偏移）
        float radius = 0.0f;
};

struct ConvexPolygon final {
        static constexpr std::size_t kMaxVertices = 8;
        std::array<algo::Vec2, kMaxVertices> vertices{};
        std::size_t count = 0;
};

struct Segment final {
        algo::Vec2 a{};
        algo::Vec2 b{};
};

class Shape {
    public:
        enum class Kind : std::uint8_t { Circle, Polygon, Segment };

        // 工厂。返回 Result<Shape>：非法输入（半径 <= 0、顶点数越界、退化、
        // 非凸）通过 atom::Error 上报，而不是静默截断或产生退化形状。
        [[nodiscard]] static auto MakeCircle(float radius) -> atom::Result<Shape>;
        [[nodiscard]] static auto MakePolygon(std::span<const algo::Vec2> vertices) -> atom::Result<Shape>;
        [[nodiscard]] static auto MakeSegment(algo::Vec2 a, algo::Vec2 b) -> atom::Result<Shape>;

        [[nodiscard]] auto GetKind() const -> Kind {
            return kind_;
        }
        // 类型化访问器：要求调用前 GetKind() 已匹配，否则 debug 断言。
        [[nodiscard]] auto GetCircle() const -> const Circle&;
        [[nodiscard]] auto GetPolygon() const -> const ConvexPolygon&;
        [[nodiscard]] auto GetSegment() const -> const Segment&;

    private:
        Kind kind_ = Kind::Circle;
        Circle circle_{};
        ConvexPolygon polygon_{};
        Segment segment_{};
};

// 面积（线段为 0）。
[[nodiscard]] auto ComputeArea(const Shape& shape) -> float;
// 局部空间质心（相对形状局部原点）。
[[nodiscard]] auto ComputeCentroid(const Shape& shape) -> algo::Vec2;
// 局部空间轴对齐包围盒。
[[nodiscard]] auto ComputeAabb(const Shape& shape) -> algo::Aabb2;
// 质量属性：density 来自 Material。质量为 0 时 inverse_* 为 0。
[[nodiscard]] auto ComputeMassProperties(const Shape& shape, float density) -> MassProperties;

} // namespace atom::physics

#endif // ATOM_PHYSICS_SHAPES_SHAPE_HPP

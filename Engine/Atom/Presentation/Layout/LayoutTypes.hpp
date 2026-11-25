// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file LayoutTypes.hpp
 * @brief Renderer-independent types used by Atom's layout module.
 * @author Romi Brooks
 * @date 2026/09/01
 */

#ifndef ATOM_LAYOUTTYPES_HPP
#define ATOM_LAYOUTTYPES_HPP

#include <cstddef>
#include <limits>

namespace atom::layout {
enum class Unit { Undefined, Points, Percent, Auto };

struct Length {
    float value = std::numeric_limits<float>::quiet_NaN();
    Unit unit = Unit::Undefined;

    [[nodiscard]] static constexpr auto Undefined() -> Length {
        return {};
    }

    [[nodiscard]] static constexpr auto Points(float value) -> Length {
        return {value, Unit::Points};
    }

    [[nodiscard]] static constexpr auto Percent(float value) -> Length {
        return {value, Unit::Percent};
    }

    [[nodiscard]] static constexpr auto Auto() -> Length {
        return {std::numeric_limits<float>::quiet_NaN(), Unit::Auto};
    }
};

struct Edges {
    Length left = Length::Points(0.0f);
    Length top = Length::Points(0.0f);
    Length right = Length::Points(0.0f);
    Length bottom = Length::Points(0.0f);

    [[nodiscard]] static constexpr auto All(Length value) -> Edges {
        return {value, value, value, value};
    }
};

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

struct Rect {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class Direction { Inherit, LeftToRight, RightToLeft };

enum class FlexDirection { Row, RowReverse, Column, ColumnReverse };

enum class Justify { FlexStart, Center, FlexEnd, SpaceBetween, SpaceAround, SpaceEvenly };

enum class Align { Auto, FlexStart, Center, FlexEnd, Stretch, Baseline, SpaceBetween, SpaceAround, SpaceEvenly };

enum class Wrap { NoWrap, Wrap, WrapReverse };

enum class PositionType { Static, Relative, Absolute };

enum class Overflow { Visible, Hidden, Scroll };

enum class MeasureMode { Undefined, Exactly, AtMost };

struct MeasureInput {
    float width = 0.0f;
    MeasureMode width_mode = MeasureMode::Undefined;
    float height = 0.0f;
    MeasureMode height_mode = MeasureMode::Undefined;
};

struct LayoutStyle {
    Direction direction = Direction::Inherit;
    FlexDirection flex_direction = FlexDirection::Row;
    Justify justify_content = Justify::FlexStart;
    Align align_content = Align::Stretch;
    Align align_items = Align::Stretch;
    Align align_self = Align::Auto;
    Wrap flex_wrap = Wrap::NoWrap;
    PositionType position_type = PositionType::Relative;
    Overflow overflow = Overflow::Visible;

    float flex_grow = 0.0f;
    float flex_shrink = 1.0f;
    Length flex_basis = Length::Auto();

    Length width = Length::Auto();
    Length height = Length::Auto();
    Length min_width = Length::Undefined();
    Length min_height = Length::Undefined();
    Length max_width = Length::Undefined();
    Length max_height = Length::Undefined();

    Edges margin{};
    Edges padding{};
    Edges position{Length::Undefined(), Length::Undefined(), Length::Undefined(), Length::Undefined()};
    float row_gap = 0.0f;
    float column_gap = 0.0f;
    float aspect_ratio = std::numeric_limits<float>::quiet_NaN();
};
} // namespace atom::layout

#endif // ATOM_LAYOUTTYPES_HPP

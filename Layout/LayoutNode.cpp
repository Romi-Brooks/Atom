// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file LayoutNode.cpp
 * @brief LayoutNode implementation backed by Yoga.
 * @author Romi Brooks
 * @date 2026/09/01
 */

#include "LayoutNode.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

#include <Log/LogSystem.hpp>
#include <yoga/Yoga.h>

namespace atom::layout {
namespace {
auto ToYoga(const Direction direction) -> YGDirection {
    switch (direction) {
    case Direction::Inherit:
        return YGDirectionInherit;
    case Direction::LeftToRight:
        return YGDirectionLTR;
    case Direction::RightToLeft:
        return YGDirectionRTL;
    }
    return YGDirectionInherit;
}

auto ToYoga(const FlexDirection direction) -> YGFlexDirection {
    switch (direction) {
    case FlexDirection::Row:
        return YGFlexDirectionRow;
    case FlexDirection::RowReverse:
        return YGFlexDirectionRowReverse;
    case FlexDirection::Column:
        return YGFlexDirectionColumn;
    case FlexDirection::ColumnReverse:
        return YGFlexDirectionColumnReverse;
    }
    return YGFlexDirectionRow;
}

auto ToYoga(const Justify justify) -> YGJustify {
    switch (justify) {
    case Justify::FlexStart:
        return YGJustifyFlexStart;
    case Justify::Center:
        return YGJustifyCenter;
    case Justify::FlexEnd:
        return YGJustifyFlexEnd;
    case Justify::SpaceBetween:
        return YGJustifySpaceBetween;
    case Justify::SpaceAround:
        return YGJustifySpaceAround;
    case Justify::SpaceEvenly:
        return YGJustifySpaceEvenly;
    }
    return YGJustifyFlexStart;
}

auto ToYoga(const Align align) -> YGAlign {
    switch (align) {
    case Align::Auto:
        return YGAlignAuto;
    case Align::FlexStart:
        return YGAlignFlexStart;
    case Align::Center:
        return YGAlignCenter;
    case Align::FlexEnd:
        return YGAlignFlexEnd;
    case Align::Stretch:
        return YGAlignStretch;
    case Align::Baseline:
        return YGAlignBaseline;
    case Align::SpaceBetween:
        return YGAlignSpaceBetween;
    case Align::SpaceAround:
        return YGAlignSpaceAround;
    case Align::SpaceEvenly:
        return YGAlignSpaceEvenly;
    }
    return YGAlignAuto;
}

auto ToYoga(const Wrap wrap) -> YGWrap {
    switch (wrap) {
    case Wrap::NoWrap:
        return YGWrapNoWrap;
    case Wrap::Wrap:
        return YGWrapWrap;
    case Wrap::WrapReverse:
        return YGWrapWrapReverse;
    }
    return YGWrapNoWrap;
}

auto ToYoga(const PositionType position_type) -> YGPositionType {
    switch (position_type) {
    case PositionType::Static:
        return YGPositionTypeStatic;
    case PositionType::Relative:
        return YGPositionTypeRelative;
    case PositionType::Absolute:
        return YGPositionTypeAbsolute;
    }
    return YGPositionTypeRelative;
}

auto ToYoga(const Overflow overflow) -> YGOverflow {
    switch (overflow) {
    case Overflow::Visible:
        return YGOverflowVisible;
    case Overflow::Hidden:
        return YGOverflowHidden;
    case Overflow::Scroll:
        return YGOverflowScroll;
    }
    return YGOverflowVisible;
}

auto ToAtom(const YGMeasureMode mode) -> MeasureMode {
    switch (mode) {
    case YGMeasureModeUndefined:
        return MeasureMode::Undefined;
    case YGMeasureModeExactly:
        return MeasureMode::Exactly;
    case YGMeasureModeAtMost:
        return MeasureMode::AtMost;
    }
    return MeasureMode::Undefined;
}

template <typename PointSetter, typename PercentSetter, typename AutoSetter>
auto ApplyLength(YGNodeRef node, const Length length, PointSetter point_setter, PercentSetter percent_setter,
                 AutoSetter auto_setter) -> void {
    switch (length.unit) {
    case Unit::Undefined:
        point_setter(node, YGUndefined);
        break;
    case Unit::Points:
        point_setter(node, length.value);
        break;
    case Unit::Percent:
        percent_setter(node, length.value);
        break;
    case Unit::Auto:
        auto_setter(node);
        break;
    }
}

auto ApplyMargin(YGNodeRef node, const YGEdge edge, const Length length) -> void {
    ApplyLength(
        node, length,
        [edge](YGNodeRef value_node, const float value) { YGNodeStyleSetMargin(value_node, edge, value); },
        [edge](YGNodeRef value_node, const float value) { YGNodeStyleSetMarginPercent(value_node, edge, value); },
        [edge](YGNodeRef value_node) { YGNodeStyleSetMarginAuto(value_node, edge); });
}

auto ApplyPadding(YGNodeRef node, const YGEdge edge, const Length length) -> void {
    if (length.unit == Unit::Auto) {
        LOG_WARNING(LogChannel::CORE, "Rejected Unit::Auto padding in a Yoga layout style");
        throw std::invalid_argument{"Layout padding cannot use Unit::Auto"};
    }
    ApplyLength(
        node, length,
        [edge](YGNodeRef value_node, const float value) { YGNodeStyleSetPadding(value_node, edge, value); },
        [edge](YGNodeRef value_node, const float value) { YGNodeStyleSetPaddingPercent(value_node, edge, value); },
        [edge](YGNodeRef) {});
}

auto ApplyPosition(YGNodeRef node, const YGEdge edge, const Length length) -> void {
    if (length.unit == Unit::Auto) {
        YGNodeStyleSetPositionAuto(node, edge);
        return;
    }
    if (length.unit == Unit::Percent) {
        YGNodeStyleSetPositionPercent(node, edge, length.value);
        return;
    }
    YGNodeStyleSetPosition(node, edge, length.unit == Unit::Undefined ? YGUndefined : length.value);
}
} // namespace

struct LayoutNode::Impl {
    Impl(const LayoutConfig& config_value, void* native_config)
        : config{config_value}, node{YGNodeNewWithConfig(static_cast<YGConfigRef>(native_config))} {
        if (node == nullptr) {
            LOG_ERROR(LogChannel::CORE, "Yoga failed to allocate a layout node");
            throw std::bad_alloc{};
        }
        YGNodeSetContext(node, this);
    }

    ~Impl() {
        YGNodeFree(node);
    }

    static auto Measure(YGNodeConstRef node, const float width, const YGMeasureMode width_mode, const float height,
                        const YGMeasureMode height_mode) -> YGSize {
        const auto* impl = static_cast<const Impl*>(YGNodeGetContext(node));
        if (impl == nullptr || !impl->measure_function) {
            return {0.0f, 0.0f};
        }

        try {
            const auto size = impl->measure_function({width, ToAtom(width_mode), height, ToAtom(height_mode)});
            return {size.width, size.height};
        } catch (...) {
            LOG_ERROR(LogChannel::CORE, "A layout measure callback threw; returning a zero size");
            return {0.0f, 0.0f};
        }
    }

    LayoutConfig config;
    YGNodeRef node = nullptr;
    MeasureFunction measure_function;
};

LayoutNode::LayoutNode(const LayoutConfig& config) : impl_{std::make_unique<Impl>(config, config.GetNativeHandle())} {}

LayoutNode::~LayoutNode() = default;

LayoutNode::LayoutNode(LayoutNode&& other) noexcept = default;

auto LayoutNode::operator=(LayoutNode&& other) noexcept -> LayoutNode& = default;

auto LayoutNode::SetStyle(const LayoutStyle& style) -> void {
    const auto node = impl_->node;
    YGNodeStyleSetDirection(node, ToYoga(style.direction));
    YGNodeStyleSetFlexDirection(node, ToYoga(style.flex_direction));
    YGNodeStyleSetJustifyContent(node, ToYoga(style.justify_content));
    YGNodeStyleSetAlignContent(node, ToYoga(style.align_content));
    YGNodeStyleSetAlignItems(node, ToYoga(style.align_items));
    YGNodeStyleSetAlignSelf(node, ToYoga(style.align_self));
    YGNodeStyleSetFlexWrap(node, ToYoga(style.flex_wrap));
    YGNodeStyleSetPositionType(node, ToYoga(style.position_type));
    YGNodeStyleSetOverflow(node, ToYoga(style.overflow));
    YGNodeStyleSetFlexGrow(node, style.flex_grow);
    YGNodeStyleSetFlexShrink(node, style.flex_shrink);

    ApplyLength(node, style.flex_basis, YGNodeStyleSetFlexBasis, YGNodeStyleSetFlexBasisPercent,
                YGNodeStyleSetFlexBasisAuto);
    ApplyLength(node, style.width, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
    ApplyLength(node, style.height, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
    ApplyLength(node, style.min_width, YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent,
                [](YGNodeRef value_node) { YGNodeStyleSetMinWidth(value_node, YGUndefined); });
    ApplyLength(node, style.min_height, YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent,
                [](YGNodeRef value_node) { YGNodeStyleSetMinHeight(value_node, YGUndefined); });
    ApplyLength(node, style.max_width, YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent,
                [](YGNodeRef value_node) { YGNodeStyleSetMaxWidth(value_node, YGUndefined); });
    ApplyLength(node, style.max_height, YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent,
                [](YGNodeRef value_node) { YGNodeStyleSetMaxHeight(value_node, YGUndefined); });

    ApplyMargin(node, YGEdgeLeft, style.margin.left);
    ApplyMargin(node, YGEdgeTop, style.margin.top);
    ApplyMargin(node, YGEdgeRight, style.margin.right);
    ApplyMargin(node, YGEdgeBottom, style.margin.bottom);
    ApplyPadding(node, YGEdgeLeft, style.padding.left);
    ApplyPadding(node, YGEdgeTop, style.padding.top);
    ApplyPadding(node, YGEdgeRight, style.padding.right);
    ApplyPadding(node, YGEdgeBottom, style.padding.bottom);
    ApplyPosition(node, YGEdgeLeft, style.position.left);
    ApplyPosition(node, YGEdgeTop, style.position.top);
    ApplyPosition(node, YGEdgeRight, style.position.right);
    ApplyPosition(node, YGEdgeBottom, style.position.bottom);

    YGNodeStyleSetGap(node, YGGutterRow, style.row_gap);
    YGNodeStyleSetGap(node, YGGutterColumn, style.column_gap);
    YGNodeStyleSetAspectRatio(node, style.aspect_ratio);
}

auto LayoutNode::InsertChild(LayoutNode& child, const std::size_t index) -> void {
    if (impl_.get() == child.impl_.get()) {
        LOG_WARNING(LogChannel::CORE, "Rejected an attempt to parent a layout node to itself");
        throw std::invalid_argument{"A layout node cannot be its own child"};
    }
    if (index > GetChildCount()) {
        LOG_WARNING(LogChannel::CORE, "Rejected an out-of-range layout child insertion index");
        throw std::out_of_range{"Layout child index is out of range"};
    }
    if (YGNodeHasMeasureFunc(impl_->node)) {
        LOG_WARNING(LogChannel::CORE, "Rejected a child insertion into a measured layout node");
        throw std::logic_error{"A measured layout node cannot have children"};
    }
    if (YGNodeGetOwner(child.impl_->node) != nullptr) {
        LOG_WARNING(LogChannel::CORE, "Rejected a layout child that already has a parent");
        throw std::logic_error{"Layout node already has a parent"};
    }
    for (auto ancestor = impl_->node; ancestor != nullptr; ancestor = YGNodeGetOwner(ancestor)) {
        if (ancestor == child.impl_->node) {
            LOG_WARNING(LogChannel::CORE, "Rejected a cycle in the layout tree");
            throw std::logic_error{"A layout tree cannot contain a cycle"};
        }
    }
    YGNodeInsertChild(impl_->node, child.impl_->node, index);
}

auto LayoutNode::AppendChild(LayoutNode& child) -> void {
    InsertChild(child, GetChildCount());
}

auto LayoutNode::RemoveChild(LayoutNode& child) -> void {
    if (YGNodeGetOwner(child.impl_->node) != impl_->node) {
        LOG_WARNING(LogChannel::CORE, "Rejected removal of a node that is not a direct child");
        throw std::logic_error{"Layout node is not a child of this parent"};
    }
    YGNodeRemoveChild(impl_->node, child.impl_->node);
}

auto LayoutNode::GetChildCount() const -> std::size_t {
    return YGNodeGetChildCount(impl_->node);
}

auto LayoutNode::GetLayout() const -> Rect {
    return {
        YGNodeLayoutGetLeft(impl_->node),
        YGNodeLayoutGetTop(impl_->node),
        YGNodeLayoutGetWidth(impl_->node),
        YGNodeLayoutGetHeight(impl_->node),
    };
}

auto LayoutNode::HasNewLayout() const -> bool {
    return YGNodeGetHasNewLayout(impl_->node);
}

auto LayoutNode::MarkLayoutSeen() -> void {
    YGNodeSetHasNewLayout(impl_->node, false);
}

auto LayoutNode::CalculateLayout(const std::optional<float> available_width,
                                 const std::optional<float> available_height, const Direction owner_direction) -> void {
    const auto width = available_width.value_or(std::numeric_limits<float>::quiet_NaN());
    const auto height = available_height.value_or(std::numeric_limits<float>::quiet_NaN());
    YGNodeCalculateLayout(impl_->node, width, height, ToYoga(owner_direction));
}

auto LayoutNode::SetMeasureFunction(MeasureFunction measure_function) -> void {
    if (!measure_function) {
        LOG_WARNING(LogChannel::CORE, "Rejected an empty layout measure callback");
        throw std::invalid_argument{"Layout measure function cannot be empty"};
    }
    if (GetChildCount() != 0) {
        LOG_WARNING(LogChannel::CORE, "Rejected a measure callback on a layout node with children");
        throw std::logic_error{"A measured layout node cannot have children"};
    }
    impl_->measure_function = std::move(measure_function);
    YGNodeSetMeasureFunc(impl_->node, Impl::Measure);
    LOG_DEBUG(LogChannel::CORE, "Installed a Yoga layout measure callback");
}

auto LayoutNode::ClearMeasureFunction() -> void {
    YGNodeSetMeasureFunc(impl_->node, nullptr);
    impl_->measure_function = {};
}

auto LayoutNode::MarkDirty() -> void {
    if (!impl_->measure_function) {
        LOG_WARNING(LogChannel::CORE, "Rejected MarkDirty on a layout node without a measure callback");
        throw std::logic_error{"Only measured layout nodes can be marked dirty"};
    }
    YGNodeMarkDirty(impl_->node);
}
} // namespace atom::layout

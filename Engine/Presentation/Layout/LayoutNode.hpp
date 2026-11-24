// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file LayoutNode.hpp
 * @brief RAII layout node that keeps Yoga out of Atom's public API.
 * @author Romi Brooks
 * @date 2026/09/01
 */

#ifndef ATOM_LAYOUTNODE_HPP
#define ATOM_LAYOUTNODE_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

#include <Layout/LayoutConfig.hpp>
#include <Layout/LayoutTypes.hpp>

namespace atom::layout {
class LayoutNode {
    public:
        using MeasureFunction = std::function<Size(const MeasureInput&)>;

        explicit LayoutNode(const LayoutConfig& config);
        ~LayoutNode();

        LayoutNode(const LayoutNode&) = delete;
        auto operator=(const LayoutNode&) -> LayoutNode& = delete;
        LayoutNode(LayoutNode&& other) noexcept;
        auto operator=(LayoutNode&& other) noexcept -> LayoutNode&;

        auto SetStyle(const LayoutStyle& style) -> void;
        auto InsertChild(LayoutNode& child, std::size_t index) -> void;
        auto AppendChild(LayoutNode& child) -> void;
        auto RemoveChild(LayoutNode& child) -> void;

        [[nodiscard]] auto GetChildCount() const -> std::size_t;
        [[nodiscard]] auto GetLayout() const -> Rect;
        [[nodiscard]] auto HasNewLayout() const -> bool;
        auto MarkLayoutSeen() -> void;

        auto CalculateLayout(std::optional<float> available_width = std::nullopt,
                             std::optional<float> available_height = std::nullopt,
                             Direction owner_direction = Direction::LeftToRight) -> void;

        auto SetMeasureFunction(MeasureFunction measure_function) -> void;
        auto ClearMeasureFunction() -> void;
        auto MarkDirty() -> void;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};
} // namespace atom::layout

#endif // ATOM_LAYOUTNODE_HPP

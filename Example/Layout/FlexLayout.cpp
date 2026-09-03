// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file FlexLayout.cpp
 * @brief Smoke example for Atom's renderer-independent Flexbox layout API.
 * @author Romi Brooks
 * @date 2026/09/01
 */

#include <cmath>

#include <Layout/LayoutConfig.hpp>
#include <Layout/LayoutNode.hpp>
#include <Layout/LayoutTypes.hpp>

namespace {
[[nodiscard]] auto NearlyEqual(const float left, const float right) -> bool {
    return std::abs(left - right) < 0.01f;
}
} // namespace

auto main() -> int {
    atom::layout::LayoutConfig config;
    config.SetPointScaleFactor(1.0f);

    atom::layout::LayoutNode root{config};
    auto root_style = atom::layout::LayoutStyle{};
    root_style.width = atom::layout::Length::Points(800.0f);
    root_style.height = atom::layout::Length::Points(600.0f);
    root_style.padding = atom::layout::Edges::All(atom::layout::Length::Points(20.0f));
    root_style.column_gap = 10.0f;
    root.SetStyle(root_style);

    atom::layout::LayoutNode sidebar{config};
    auto sidebar_style = atom::layout::LayoutStyle{};
    sidebar_style.width = atom::layout::Length::Points(200.0f);
    sidebar.SetStyle(sidebar_style);

    atom::layout::LayoutNode content{config};
    auto content_style = atom::layout::LayoutStyle{};
    content_style.flex_grow = 1.0f;
    content.SetStyle(content_style);

    root.AppendChild(sidebar);
    root.AppendChild(content);
    root.CalculateLayout();

    const auto sidebar_layout = sidebar.GetLayout();
    const auto content_layout = content.GetLayout();
    const auto flex_layout_is_valid =
        NearlyEqual(sidebar_layout.left, 20.0f) && NearlyEqual(sidebar_layout.width, 200.0f) &&
        NearlyEqual(content_layout.left, 230.0f) && NearlyEqual(content_layout.width, 550.0f);

    atom::layout::LayoutNode measured_text{config};
    measured_text.SetMeasureFunction([](const atom::layout::MeasureInput& input) {
        const auto natural_width = 320.0f;
        const auto width = input.width_mode == atom::layout::MeasureMode::AtMost ? input.width : natural_width;
        return atom::layout::Size{width, 24.0f};
    });
    measured_text.CalculateLayout(120.0f);
    const auto measured_layout = measured_text.GetLayout();
    const auto measurement_is_valid =
        NearlyEqual(measured_layout.width, 120.0f) && NearlyEqual(measured_layout.height, 24.0f);

    return flex_layout_is_valid && measurement_is_valid ? 0 : 1;
}

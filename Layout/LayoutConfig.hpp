// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file LayoutConfig.hpp
 * @brief Shared configuration for Atom layout nodes.
 * @author Romi Brooks
 * @date 2026/09/01
 */

#ifndef ATOM_LAYOUTCONFIG_HPP
#define ATOM_LAYOUTCONFIG_HPP

#include <memory>

namespace atom::layout {
class LayoutNode;

class LayoutConfig {
    public:
        explicit LayoutConfig(bool use_web_defaults = true);
        ~LayoutConfig();

        LayoutConfig(const LayoutConfig&) = default;
        auto operator=(const LayoutConfig&) -> LayoutConfig& = default;
        LayoutConfig(LayoutConfig&&) noexcept = default;
        auto operator=(LayoutConfig&&) noexcept -> LayoutConfig& = default;

        auto SetPointScaleFactor(float scale_factor) -> void;
        [[nodiscard]] auto GetPointScaleFactor() const -> float;
        [[nodiscard]] auto UsesWebDefaults() const -> bool;

    private:
        struct Impl;
        std::shared_ptr<Impl> impl_;

        [[nodiscard]] auto GetNativeHandle() const -> void*;

        friend class LayoutNode;
};
} // namespace atom::layout

#endif // ATOM_LAYOUTCONFIG_HPP

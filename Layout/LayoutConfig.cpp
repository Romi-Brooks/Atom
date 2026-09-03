// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file LayoutConfig.cpp
 * @brief LayoutConfig implementation backed by Yoga.
 * @author Romi Brooks
 * @date 2026/09/01
 */

#include "LayoutConfig.hpp"

#include <stdexcept>
#include <string>

#include <Log/LogSystem.hpp>
#include <yoga/Yoga.h>

namespace atom::layout {
struct LayoutConfig::Impl {
    explicit Impl(const bool use_web_defaults) : config{YGConfigNew()} {
        if (config == nullptr) {
            LOG_ERROR(LogChannel::CORE, "Yoga failed to allocate a layout configuration");
            throw std::bad_alloc{};
        }
        YGConfigSetUseWebDefaults(config, use_web_defaults);
        LOG_DEBUG(LogChannel::CORE, std::string{"Created Yoga layout configuration (web defaults="} +
                                        (use_web_defaults ? "true" : "false") + ")");
    }

    ~Impl() {
        YGConfigFree(config);
    }

    YGConfigRef config = nullptr;
};

LayoutConfig::LayoutConfig(const bool use_web_defaults) : impl_{std::make_shared<Impl>(use_web_defaults)} {}

LayoutConfig::~LayoutConfig() = default;

auto LayoutConfig::SetPointScaleFactor(const float scale_factor) -> void {
    if (scale_factor < 0.0f) {
        LOG_WARNING(LogChannel::CORE, "Rejected negative Yoga point scale factor: " + std::to_string(scale_factor));
        throw std::invalid_argument{"Layout point scale factor cannot be negative"};
    }
    YGConfigSetPointScaleFactor(impl_->config, scale_factor);
    LOG_DEBUG(LogChannel::CORE, "Set Yoga point scale factor to " + std::to_string(scale_factor));
}

auto LayoutConfig::GetPointScaleFactor() const -> float {
    return YGConfigGetPointScaleFactor(impl_->config);
}

auto LayoutConfig::UsesWebDefaults() const -> bool {
    return YGConfigGetUseWebDefaults(impl_->config);
}

auto LayoutConfig::GetNativeHandle() const -> void* {
    return impl_->config;
}
} // namespace atom::layout

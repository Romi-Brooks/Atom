// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "DebugPanel.hpp"

#include <Log/LogSystem.hpp>
#include <Window/OverlayManager.hpp>
#include <Window/RenderWindow.hpp>

namespace atom::debugger {

DebugPanel::~DebugPanel() {
    if (attached_)
        Detach();
}

auto DebugPanel::OnAttach(atom::RenderWindow&) -> void {}

auto DebugPanel::OnDetach() -> void {}

auto DebugPanel::Attach(atom::RenderWindow& window) -> void {
    if (attached_)
        return;

    target_window_ = &window;
    if (!window.GetIWindow() || !window.GetRenderDevice()) {
        LOG_WARNING(atom::log::debugger::ImGui,
                    "DebugPanel attach requires an initialized render window and device");
        target_window_ = nullptr;
        return;
    }

    auto& overlay_manager = window.GetOverlayManager();
    overlay_connection_ = std::make_unique<OverlayConnection>(overlay_manager.AddPanel([this] {
        if (enabled_)
            OnDrawOverlay();
    }));
    if (!overlay_connection_->IsConnected()) {
        LOG_ERROR(atom::log::debugger::ImGui,
                  "DebugPanel attach failed for render backend '" + window.GetBackendId() + "'");
        overlay_connection_.reset();
        target_window_ = nullptr;
        return;
    }

    update_connection_ = std::make_unique<atom::ListenerConnection>(window.AddUpdateListener([this](float delta_time) {
        frame_count_++;
        fps_accumulator_ += delta_time;
        if (fps_accumulator_ >= 1.0f) {
            fps_display_ = static_cast<float>(frame_count_) / fps_accumulator_;
            frame_count_ = 0;
            fps_accumulator_ = 0.0f;
        }
    }));

    OnAttach(window);
    enabled_ = true;
    attached_ = true;
    LOG_INFO(atom::log::debugger::ImGui, "DebugPanel attached to render backend '" + window.GetBackendId() + "'");
}

auto DebugPanel::Detach() -> void {
    if (!attached_ || !target_window_)
        return;

    OnDetach();
    update_connection_.reset();
    overlay_connection_.reset();

    target_window_ = nullptr;
    attached_ = false;
    LOG_INFO(atom::log::debugger::ImGui, "DebugPanel detached");
}

} // namespace atom::debugger

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "DebugPanel.hpp"

#include <string>

#include <Log/LogSystem.hpp>
#include <Window/OverlayManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/ScreenManager.hpp>

namespace atom::debugger {

DebugPanel::~DebugPanel() {
    // Safety net only. Virtual GetPanelName/OnDetach are not safe here (the
    // derived object is already destroyed). Prefer Detach() in the derived
    // destructor so hooks still dispatch; this path uses the name cached at Attach.
    if (!attached_)
        return;
    update_connection_.reset();
    overlay_connection_.reset();
    target_window_ = nullptr;
    bound_screen_ = nullptr;
    attached_ = false;
    LOG_INFO(atom::log::debugger::ImGui, panel_name_ + " detached (" + panel_scope_ + ")");
}

auto DebugPanel::OnAttach(atom::RenderWindow&) -> bool {
    return true;
}

auto DebugPanel::OnDetach() -> void {}

auto DebugPanel::Attach(atom::RenderWindow& window) -> void {
    AttachScoped(window, nullptr);
}

auto DebugPanel::Attach(atom::RenderWindow& window, atom::Screen& screen) -> void {
    AttachScoped(window, &screen);
}

auto DebugPanel::AttachScoped(atom::RenderWindow& window, atom::Screen* screen) -> void {
    if (attached_)
        return;

#if !ATOM_ENABLE_DEBUGGER
    (void)window;
    (void)screen;
    return;
#else
    target_window_ = &window;
    bound_screen_ = screen;
    if (!window.GetIWindow() || !window.GetRenderDevice()) {
        LOG_WARNING(atom::log::debugger::ImGui,
                    "DebugPanel attach requires an initialized render window and device");
        target_window_ = nullptr;
        bound_screen_ = nullptr;
        return;
    }

    auto& overlay_manager = window.GetOverlayManager();
    overlay_connection_ = std::make_unique<OverlayConnection>(overlay_manager.AddPanel([this] {
        if (!enabled_ || !IsScopeVisible())
            return;
        OnDrawOverlay();
    }));
    if (!overlay_connection_->IsConnected()) {
        LOG_ERROR(atom::log::debugger::ImGui,
                  std::string{GetPanelName()} + " attach failed for render backend '" +
                      window.GetBackendId() + "'");
        overlay_connection_.reset();
        target_window_ = nullptr;
        bound_screen_ = nullptr;
        return;
    }

    // Snapshot the derived name/scope before any later detach from ~DebugPanel.
    panel_name_ = GetPanelName();
    panel_scope_ = screen ? "screen-scoped" : "window-scoped";

    update_connection_ = std::make_unique<atom::ListenerConnection>(window.AddUpdateListener([this](float delta_time) {
        frame_count_++;
        fps_accumulator_ += delta_time;
        if (fps_accumulator_ >= 1.0f) {
            fps_display_ = static_cast<float>(frame_count_) / fps_accumulator_;
            frame_count_ = 0;
            fps_accumulator_ = 0.0f;
        }
    }));

    if (!OnAttach(window)) {
        update_connection_.reset();
        overlay_connection_.reset();
        target_window_ = nullptr;
        bound_screen_ = nullptr;
        return;
    }

    enabled_ = true;
    attached_ = true;
    std::string target;
    if (screen) {
        auto screen_name = atom::ScreenManager::GetInstance().GetScreenName(screen);
        if (screen_name.empty())
            screen_name = "<unnamed>";
        target = "render screen '" + screen_name + "' (" + panel_scope_ + ")";
    } else {
        target = "render window '" + window.GetName() + "' (" + panel_scope_ + ")";
    }
    LOG_INFO(atom::log::debugger::ImGui, panel_name_ + " attached to " + target);
#endif
}

auto DebugPanel::Detach() -> void {
    if (!attached_ || !target_window_)
        return;

#if !ATOM_ENABLE_DEBUGGER
    attached_ = false;
    target_window_ = nullptr;
    bound_screen_ = nullptr;
    return;
#else
    OnDetach();
    update_connection_.reset();
    overlay_connection_.reset();

    target_window_ = nullptr;
    bound_screen_ = nullptr;
    attached_ = false;
    LOG_INFO(atom::log::debugger::ImGui, panel_name_ + " detached (" + panel_scope_ + ")");
#endif
}

auto DebugPanel::IsScopeVisible() const -> bool {
    if (!bound_screen_)
        return true;
    return atom::ScreenManager::GetInstance().IsScreenVisible(bound_screen_);
}

} // namespace atom::debugger

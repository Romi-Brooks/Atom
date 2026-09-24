// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "DebugPanel.hpp"

#include <string>
#include <utility>

#include <Log/LogSystem.hpp>
#include <Window/OverlayManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/ScreenManager.hpp>

namespace atom::debugger {
DebugPanel::DebugPanel(std::string panel_name) : panel_name_(std::move(panel_name)) {}

DebugPanel::~DebugPanel() {
    if (!attached_) { return; }
    RollbackAttach();
    attached_ = false;
    LOG_INFO(atom::log::debugger::ImGui, panel_name_ + " detached (" + GetPanelScopeName() + ")");
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
        RollbackAttach();
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
                  GetPanelName() + " attach failed for render backend '" +
                      window.GetBackendId() + "'");
        RollbackAttach();
        return;
    }

    if (!OnAttach(window)) {
        RollbackAttach();
        return;
    }

    attached_ = true;

    // get the type of panel scope
    panel_scope_ = screen ? PanelScope::Screen : PanelScope::Window;

    std::string target;
    if (screen) {
        auto screen_name = atom::ScreenManager::GetInstance().GetScreenName(screen);
        if (screen_name.empty())
            screen_name = "<unnamed>";
        target = "render screen '" + screen_name + "' (" + GetPanelScopeName() + ")";
    } else {
        target = "render window '" + window.GetName() + "' (" + GetPanelScopeName() + ")";
    }
    LOG_INFO(atom::log::debugger::ImGui, panel_name_ + " attached to " + target);
#endif
}

auto DebugPanel::Detach() -> void {
    if (!attached_) { return; }

    OnDetach();
    RollbackAttach();
    attached_ = false;
    LOG_INFO(atom::log::debugger::ImGui, panel_name_ + " detached (" + GetPanelScopeName() + ")");
}

auto DebugPanel::IsScopeVisible() const -> bool {
    if (!bound_screen_)
        return true;
    return atom::ScreenManager::GetInstance().IsScreenVisible(bound_screen_);
}

auto DebugPanel::GetPanelScopeName() const -> const char* {
    switch (panel_scope_) {
    case PanelScope::Window: return "Window";
    case PanelScope::Screen: return "Screen";
    }
    return "unknown";
}

auto DebugPanel::RollbackAttach() -> void {
    overlay_connection_.reset();
    target_window_ = nullptr;
    bound_screen_ = nullptr;
}

} // namespace atom::debugger

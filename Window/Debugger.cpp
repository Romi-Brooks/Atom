/**
  * @file           : Debugger.cpp
  * @brief          : Shared ImGui debug panel registration.
  */

#include "Debugger.hpp"

#include <Log/LogSystem.hpp>
#include <Window/LogDebugger.hpp>
#include <Window/OverlayManager.hpp>
#include <Window/RenderWindow.hpp>

namespace atom {

Debugger::~Debugger() {
    if (attached_)
        Detach();
}

auto Debugger::Attach(RenderWindow& window) -> void {
    if (attached_)
        return;

    target_window_ = &window;
    if (!window.GetIWindow() || !window.GetRenderDevice()) {
        LOG_WARNING(atom::debugger::LogChannel::IMGUI,
                    "Debugger attach requires an initialized render window and device");
        target_window_ = nullptr;
        return;
    }

    auto& overlay_manager = window.GetOverlayManager();
    overlay_connection_ = std::make_unique<atom::debugger::OverlayConnection>(
        overlay_manager.AddPanel([this] {
            if (enabled_)
                OnDrawOverlay();
        }));
    if (!overlay_connection_->IsConnected()) {
        LOG_ERROR(atom::debugger::LogChannel::IMGUI,
                  "Debugger attach failed for render backend '" + window.GetBackendId() + "'");
        overlay_connection_.reset();
        target_window_ = nullptr;
        return;
    }

    update_connection_ = std::make_unique<ListenerConnection>(window.AddUpdateListener([this](float delta_time) {
        frame_count_++;
        fps_accumulator_ += delta_time;
        if (fps_accumulator_ >= 1.0f) {
            fps_display_ = static_cast<float>(frame_count_) / fps_accumulator_;
            frame_count_ = 0;
            fps_accumulator_ = 0.0f;
        }
    }));

    enabled_ = true;
    attached_ = true;
    if (logger_enabled_) {
        log_debugger_ = std::make_unique<LogDebugger>();
        log_debugger_->Attach(window);
    }
    LOG_INFO(atom::debugger::LogChannel::IMGUI, "Debugger attached to render backend '" + window.GetBackendId() + "'");
}

auto Debugger::Detach() -> void {
    if (!attached_ || !target_window_)
        return;

    if (log_debugger_)
        log_debugger_->Detach();
    update_connection_.reset();
    overlay_connection_.reset();

    target_window_ = nullptr;
    attached_ = false;
    LOG_INFO(atom::debugger::LogChannel::IMGUI, "Debugger detached");
}

auto Debugger::SetLoggerEnabled(const bool enabled) -> void {
    logger_enabled_ = enabled;
    if (!attached_ || !target_window_)
        return;

    if (enabled) {
        if (!log_debugger_)
            log_debugger_ = std::make_unique<LogDebugger>();
        log_debugger_->Attach(*target_window_);
    } else if (log_debugger_) {
        log_debugger_->Detach();
    }
}

} // namespace atom

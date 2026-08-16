/**
  * @file           : Debugger.cpp
  * @author         : Romi Brooks
  * @brief          : Debug overlay implementation (backend-agnostic)
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Debugger.hpp"

// Engine Headers
#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Registry/DebugImGuiBackendRegistry.hpp>
#include <Window/RenderWindow.hpp>

namespace atom {

Debugger::~Debugger() {
    if (attached_) {
        Detach();
    }
}

auto Debugger::Attach(RenderWindow& window) -> void {
    if (attached_)
        return;

    target_window_ = &window;

    auto* renderWindow = window.GetIRenderWindow();
    if (!renderWindow) {
        target_window_ = nullptr;
        return;
    }

    // Select the ImGui backend through the contract registry (registered by
    // the runtime layer for the window's backend id). No SDL3/ImGui types
    // appear in this file: all coupling lives in Backend/SDL3/Debug/.
    imgui_backend_ = DebugImGuiBackendRegistry::GetInstance().Create(window.GetBackendId(), *renderWindow);
    if (!imgui_backend_ || !imgui_backend_->Initialize()) {
        imgui_backend_.reset();
        target_window_ = nullptr;
        return;
    }

    // Register listeners with RAII connections. Each connection removes only
    // this debugger's own listener on destruction / Detach(); other listeners
    // (other debuggers, user overlays) are never touched.
    raw_event_connection_ = std::make_unique<ListenerConnection>(window.AddRawEventListener([this](const void* rawEvent) {
        imgui_backend_->ProcessRawEvent(rawEvent);
    }));

    update_connection_ = std::make_unique<ListenerConnection>(window.AddUpdateListener([this](float deltaTime) {
        imgui_backend_->NewFrame();

        // Track FPS
        frame_count_++;
        fps_accumulator_ += deltaTime;
        if (fps_accumulator_ >= 1.0f) {
            fps_display_ = static_cast<float>(frame_count_) / fps_accumulator_;
            frame_count_ = 0;
            fps_accumulator_ = 0.0f;
        }
    }));

    overlay_connection_ = std::make_unique<ListenerConnection>(window.AddOverlayListener([this]() {
        OnDrawOverlay();
        imgui_backend_->Render();
    }));

    // Hook shutdown (single-shot, invoked by RenderWindow::Shutdown)
    shutdown_connection_ = std::make_unique<ListenerConnection>(window.AddShutdownListener([this]() {
        if (imgui_backend_) {
            imgui_backend_->Shutdown();
            imgui_backend_.reset();
        }
        imgui_shutdown_ = true;
    }));

    attached_ = true;
}

auto Debugger::Detach() -> void {
    if (!attached_ || !target_window_)
        return;

    // Remove only this debugger's own listeners; other listeners stay intact.
    raw_event_connection_.reset();
    update_connection_.reset();
    overlay_connection_.reset();
    shutdown_connection_.reset();

    // Shutdown ImGui backend (skip if already done by the shutdown listener)
    if (!imgui_shutdown_ && imgui_backend_) {
        imgui_backend_->Shutdown();
    }
    imgui_backend_.reset();

    target_window_ = nullptr;
    attached_ = false;
}

} // namespace atom

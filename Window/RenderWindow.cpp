/**
  * @file           : RenderWindow.cpp
  * @author         : Romi Brooks
  * @brief          : Main render window singleton implementation
  * @attention      :
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include "RenderWindow.hpp"

#include <Backend/Extension/RenderBackendRegistry.hpp>
#include <Backend/Runtime/RenderBackendRuntime.hpp>
#include <Log/LogSystem.hpp>

namespace atom {

RenderWindow::~RenderWindow() {
    // OverlayManager owns RAII callbacks into RenderWindow's listener
    // storage. Release it while that storage is still alive.
    overlay_manager_.reset();
}

auto RenderWindow::GetInstance() -> RenderWindow& {
    static RenderWindow instance;
    return instance;
}

auto RenderWindow::ProcessEvents(const ScreenManager& screenManager) -> void {
    auto& window = backend_->Window();
    auto& device = backend_->Device();
    auto last_time = window.GetTimeSeconds();

    while (window.IsOpen()) {
        const auto frame_start = window.GetTimeSeconds();

        ProcessEvents(screenManager);

        if (!window.IsOpen())
            break;

        const auto delta_time = static_cast<float>(frame_start - last_time);
        last_time = frame_start;

        // Update game logic before frame extensions and rendering so that
        // state changes are visible in the same frame.
        screenManager.Update(delta_time);

        for (const auto& entry : update_listeners_) {
            entry.fn(delta_time);
        }

        if (!device.BeginFrame()) {
            window.WaitForNextFrame();
            continue;
        }
        device.Clear(atom::render::Color::Black());
        screenManager.Render(device);

        for (const auto& entry : overlay_listeners_) {
            entry.fn();
        }

        device.EndFrame();
        window.WaitForNextFrame();
    }
    Shutdown();
}

auto RenderWindow::SetFPS(const unsigned int fps) -> void {
    fps_ = fps;
    if (backend_)
        backend_->Window().SetFPS(fps);
}

auto RenderWindow::GetIWindow() -> atom::window::IWindow* {
    return backend_ ? &backend_->Window() : nullptr;
}
auto RenderWindow::GetRenderDevice() -> atom::render::IRenderDevice* {
    return backend_ ? &backend_->Device() : nullptr;
}

auto RenderWindow::GetFPS() const -> unsigned {
    return fps_;
}

auto RenderWindow::SetVSync(const bool enabled) -> bool {
    return backend_ != nullptr && backend_->Device().SetVSync(enabled);
}

auto RenderWindow::IsVSyncEnabled() const -> bool {
    return backend_ != nullptr && backend_->Device().IsVSyncEnabled();
}

auto RenderWindow::IsOpen() const -> bool {
    return backend_ && backend_->Window().IsOpen();
}

auto RenderWindow::Shutdown() -> void {
    // Single-shot shutdown listeners: Run() calls Shutdown() again after the
    // loop exits, and screens may also request shutdown mid-frame.
    if (!shutdown_notified_) {
        shutdown_notified_ = true;
        for (const auto& entry : shutdown_listeners_) {
            entry.fn();
        }
    }
    if (backend_) {
        backend_->Shutdown();
    }
    pending_resize_.reset();
}

// ── Listener registry ───────────────────────────────────────────────
auto RenderWindow::AddEventListener(EventListener listener) -> ListenerConnection {
    return AddListener(event_listeners_, std::move(listener));
}

auto RenderWindow::AddUpdateListener(UpdateListener listener) -> ListenerConnection {
    return AddListener(update_listeners_, std::move(listener));
}

auto RenderWindow::AddOverlayListener(OverlayListener listener) -> ListenerConnection {
    return AddListener(overlay_listeners_, std::move(listener));
}

auto RenderWindow::AddResizeListener(ResizeListener listener) -> ListenerConnection {
    return AddListener(resize_listeners_, std::move(listener));
}

auto RenderWindow::AddShutdownListener(ShutdownListener listener) -> ListenerConnection {
    return AddListener(shutdown_listeners_, std::move(listener));
}

auto RenderWindow::GetBackendId() const -> const std::string& {
    return backend_id_;
}

auto RenderWindow::GetOverlayManager() -> atom::debugger::OverlayManager& {
    if (!overlay_manager_)
        overlay_manager_ = std::make_unique<atom::debugger::OverlayManager>(*this);
    return *overlay_manager_;
}

} // namespace atom

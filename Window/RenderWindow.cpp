/**
  * @file           : RenderWindow.cpp
  * @author         : Romi Brooks
  * @brief          : Main render window singleton implementation
  * @attention      :
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include "RenderWindow.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include <Backend/Registry/RenderBackendRegistry.hpp>
#include <Backend/Runtime/RenderBackendRuntime.hpp>
#include <Log/LogSystem.hpp>

namespace atom {

auto RenderWindow::GetInstance() -> RenderWindow& {
    static RenderWindow instance;
    return instance;
}

auto RenderWindow::ProcessEvents(const ScreenManager& screenManager) -> void {
    while (auto event = window_->PollEvent()) {
        if (event->type != EventType::None) {
            for (const auto& entry : event_listeners_) {
                entry.fn(*event);
            }
        }

        if (event->type == EventType::Resized) {
            const auto& resize = std::get<ResizeEvent>(event->data);
            // Backend first (Vulkan swapchain recreation), then engine listeners.
            window_->HandleResize(resize.width, resize.height);
            for (const auto& entry : resize_listeners_) {
                entry.fn(resize.width, resize.height);
            }
        }

        screenManager.HandleEvent(*event);

        if (!window_->IsOpen())
            break;

        if (event->type == EventType::Closed) {
            window_->Shutdown();
            break;
        }
    }
}

auto RenderWindow::Initialize(const std::string& title, Vec2 resolution, std::string_view backendId) -> void {
    // The runtime layer owns the concrete backends; this facade only consumes
    // the registry and the IRenderWindow interface.
    RenderBackendRuntime::GetInstance().EnsureDefaultRenderBackend();
    backend_id_ = std::string(backendId);

    // Fresh window session: shutdown listeners must fire again on the next
    // Shutdown().
    shutdown_notified_ = false;

    auto& registry = RenderBackendRegistry::GetInstance();
    window_ = registry.CreateWindow(backendId);
    if (!window_) {
        LOG_ERROR(atom::core::LogChannel::WINDOW, "Render backend '" + backend_id_ + "' is not registered");
        return;
    }

    window_->Initialize(title, resolution);

    // Forward the facade's raw-event listeners into the backend. The lambda
    // reads the listener list at call time, so listeners registered after
    // Initialize() (e.g. Debugger::Attach) take effect immediately. All other
    // hooks are invoked by the facade itself and need no backend forwarding.
    window_->SetRawEventHook([this](const void* rawEvent) {
        for (const auto& entry : raw_event_listeners_) {
            entry.fn(rawEvent);
        }
    });
}

auto RenderWindow::Run() -> void {
    if (!window_) {
        throw std::runtime_error("Render window not initialized (backend unavailable).");
    }

    const auto& screenManager = atom::ScreenManager::GetInstance();
    if (screenManager.GetCurrentScreenName().empty()) {
        throw std::runtime_error("No current screen set. Cannot run application.");
    }

    const auto frameDuration = std::chrono::duration<float, std::milli>(1000.0f / static_cast<float>(fps_));
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (window_->IsOpen()) {
        const auto frameStart = std::chrono::high_resolution_clock::now();

        ProcessEvents(screenManager);

        if (!window_->IsOpen())
            break;

        const float deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        // Update game logic before frame extensions and rendering so that
        // state changes are visible in the same frame.
        screenManager.Update(deltaTime);

        for (const auto& entry : update_listeners_) {
            entry.fn(deltaTime);
        }

        window_->Clear(atom::Color::Black());
        screenManager.Render(*window_);

        for (const auto& entry : overlay_listeners_) {
            entry.fn();
        }

        window_->Display();

        const auto frameEnd = std::chrono::high_resolution_clock::now();
        const auto frameTime = std::chrono::duration<float, std::milli>(frameEnd - frameStart);
        if (frameTime < frameDuration) {
            std::this_thread::sleep_for(frameDuration - frameTime);
        }
    }
    Shutdown();
}

auto RenderWindow::SetFPS(const unsigned int fps) -> void {
    fps_ = fps;
    if (window_)
        window_->SetFPS(fps);
}

auto RenderWindow::GetIRenderWindow() -> IRenderWindow* {
    return window_.get();
}

auto RenderWindow::GetFPS() const -> unsigned {
    return fps_;
}

auto RenderWindow::IsOpen() const -> bool {
    return window_ && window_->IsOpen();
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
    if (window_) {
        window_->Shutdown();
    }
}

// ── Listener registry ───────────────────────────────────────────────
auto RenderWindow::AddRawEventListener(RawEventListener listener) -> ListenerConnection {
    return AddListener(raw_event_listeners_, std::move(listener));
}

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

} // namespace atom

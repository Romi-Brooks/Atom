/**
  * @file           : RenderWindow.cpp
  * @author         : Romi Brooks
  * @brief          : Main render window singleton implementation
  * @attention      :
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include "RenderWindow.hpp"

#include <Backend/Registry/RenderBackendRegistry.hpp>
#include <Backend/Runtime/RenderBackendRuntime.hpp>
#include <Log/LogSystem.hpp>

namespace atom {

auto RenderWindow::GetInstance() -> RenderWindow& {
    static RenderWindow instance;
    return instance;
}

auto RenderWindow::ProcessEvents(const ScreenManager& screenManager) -> void {
    auto& window = backend_->Window();
    while (auto event = window.PollEvent()) {
        if (event->type == atom::window::EventType::None)
            continue;

        for (const auto& entry : event_listeners_) {
            entry.fn(*event);
        }

        if (event->type == atom::window::EventType::Resized) {
            const auto& resize = std::get<atom::window::ResizeEvent>(event->data);
            // Backend first (Vulkan swapchain recreation), then engine listeners.
            backend_->Device().HandleResize(resize.width, resize.height);
            for (const auto& entry : resize_listeners_) {
                entry.fn(resize.width, resize.height);
            }
        }

        screenManager.HandleEvent(*event);

        if (!window.IsOpen())
            break;
    }
}

auto RenderWindow::Initialize(const std::string& title, atom::algo::Vec2 resolution, std::string_view backendId)
    -> void {
    // The runtime layer owns concrete backends; this facade only consumes the
    // registry and the IRenderBackend/IWindow/IRenderDevice contracts.
    atom::backend::RenderBackendRuntime::GetInstance().EnsureDefaultRenderBackend();
    backend_id_ = std::string(backendId);

    // Fresh window session: shutdown listeners must fire again on the next
    // Shutdown().
    shutdown_notified_ = false;

    auto& registry = atom::backend::RenderBackendRegistry::GetInstance();
    backend_ = registry.CreateBackend(backendId);
    if (!backend_) {
        LOG_ERROR(atom::core::LogChannel::WINDOW, "Render backend '" + backend_id_ + "' is not registered");
        return;
    }

    if (!backend_->Initialize(title, resolution)) {
        backend_.reset();
        return;
    }
    backend_->Window().SetFPS(fps_);

    // Forward the facade's raw-event listeners into the backend. The lambda
    // reads the listener list at call time, so listeners registered after
    // Initialize() (e.g. Debugger::Attach) take effect immediately. All other
    // hooks are invoked by the facade itself and need no backend forwarding.
    backend_->Window().SetRawEventHook([this](const void* rawEvent) {
        for (const auto& entry : raw_event_listeners_) {
            entry.fn(rawEvent);
        }
    });
}

auto RenderWindow::Run() -> void {
    if (!backend_) {
        throw std::runtime_error("Render window not initialized (backend unavailable).");
    }

    const auto& screenManager = atom::ScreenManager::GetInstance();
    if (screenManager.GetCurrentScreenName().empty()) {
        throw std::runtime_error("No current screen set. Cannot run application.");
    }

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

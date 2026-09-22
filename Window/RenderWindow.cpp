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
            pending_resize_ = PendingResize{resize.width, resize.height, window.GetTimeSeconds()};
        }

        screenManager.HandleEvent(*event);

        if (!window.IsOpen())
            break;
    }

    if (!window.IsOpen() || !pending_resize_)
        return;
    const double now = window.GetTimeSeconds();
    if (now - pending_resize_->last_change_seconds < kResizeSettleDelaySeconds)
        return;

    auto settled = *pending_resize_;
    pending_resize_.reset();
    // The backend's regular resize event may report logical dimensions on a
    // high-DPI display. The settled notification is deliberately physical:
    // it is commonly used for logs, persisted window size and render targets.
    const auto physicalSize = window.GetSize();
    if (physicalSize.GetX() > 0.0f && physicalSize.GetY() > 0.0f) {
        settled.width = static_cast<uint32_t>(physicalSize.GetX());
        settled.height = static_cast<uint32_t>(physicalSize.GetY());
    }
    atom::window::IEvent settledEvent{};
    settledEvent.type = atom::window::EventType::ResizeSettled;
    settledEvent.data = atom::window::ResizeEvent{settled.width, settled.height};
    LOG_INFO(atom::log::core::Window,
             "Window resize settled: " + std::to_string(settled.width) + "x" + std::to_string(settled.height));
    for (const auto& entry : event_listeners_) {
        entry.fn(settledEvent);
    }
    screenManager.HandleEvent(settledEvent);
}

auto RenderWindow::Initialize(const std::string& title, atom::algo::Vec2 resolution,
                              const atom::backend::RenderBackendId backendId)
    -> void {
    // The runtime layer owns concrete backends; this facade only consumes the
    // registry and the IRenderBackend/IWindow/IRenderDevice contracts.
    atom::backend::RenderBackendRuntime::GetInstance().EnsureDefaultRenderBackend();
    const auto backend_id = atom::backend::ToString(backendId);
    backend_id_ = std::string{backend_id};

    // Fresh window session: shutdown listeners must fire again on the next
    // Shutdown().
    shutdown_notified_ = false;
    pending_resize_.reset();

    auto& registry = atom::backend::RenderBackendRegistry::GetInstance();
    backend_ = registry.CreateBackend(backend_id);
    if (!backend_) {
        LOG_ERROR(atom::log::core::Window, "Render backend '" + backend_id_ + "' is not registered");
        return;
    }

    if (!backend_->Initialize(title, resolution)) {
        backend_.reset();
        return;
    }
    backend_->Window().SetFPS(fps_);

    // Host clock is owned by the backend (SDL_GetPerformanceCounter). Domain
    // clocks (game/physics/render/ui/audio) derive from it — see CORE-001.
    atom::time::TimeSystem::GetInstance().Initialize(backend_->TimeSource());

    if (overlay_manager_)
        overlay_manager_->OnRenderWindowInitialized();

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
    auto& time_system = atom::time::TimeSystem::GetInstance();

    while (window.IsOpen()) {
        // CORE-001 frame order:
        //   Event -> FixedUpdate x N -> Variable Update -> Render -> Present
        const auto frame = time_system.Tick();

        ProcessEvents(screenManager);

        if (!window.IsOpen())
            break;

        const auto* game_clock = time_system.FindDomain(atom::time::domain::kGame);
        const auto* render_clock = time_system.FindDomain(atom::time::domain::kRender);

        // Fixed-step simulation (game domain). Physics may own its own domain.
        if (game_clock) {
            const auto& tick = game_clock->LastTick();
            const auto step_seconds =
                tick.fixed_step_ns > 0 ? atom::time::ToFloatSeconds(tick.fixed_step_ns) : 0.0f;
            for (std::uint32_t step = 0; step < tick.steps; ++step) {
                screenManager.FixedUpdate(step_seconds);
                for (const auto& entry : fixed_update_listeners_) {
                    entry.fn(step_seconds);
                }
            }
        }

        // Variable update (render domain): animations, presentation.
        const auto variable_delta = render_clock ? render_clock->LastTick().delta_ns : frame.host_delta_ns;
        const auto delta_time = atom::time::ToFloatSeconds(variable_delta);
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
    atom::time::TimeSystem::GetInstance().Shutdown();
    pending_resize_.reset();
}

// ── Listener registry ───────────────────────────────────────────────
auto RenderWindow::AddEventListener(EventListener listener) -> ListenerConnection {
    return AddListener(event_listeners_, std::move(listener));
}

auto RenderWindow::AddUpdateListener(UpdateListener listener) -> ListenerConnection {
    return AddListener(update_listeners_, std::move(listener));
}

auto RenderWindow::AddFixedUpdateListener(FixedUpdateListener listener) -> ListenerConnection {
    return AddListener(fixed_update_listeners_, std::move(listener));
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

auto RenderWindow::GetTimeSystem() -> atom::time::TimeSystem& {
    return atom::time::TimeSystem::GetInstance();
}

auto RenderWindow::GetOverlayManager() -> atom::debugger::OverlayManager& {
    if (!overlay_manager_)
        overlay_manager_ = std::make_unique<atom::debugger::OverlayManager>(*this);
    return *overlay_manager_;
}

} // namespace atom

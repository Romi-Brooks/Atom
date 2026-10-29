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
#include <Backend/Composition/RenderBackendRuntime.hpp>
#include <Error/Error.hpp>
#include <Log/LogSystem.hpp>

namespace atom {

RenderWindow::~RenderWindow() {
    LOG_INFO(atom::log::core::Window, "Render window destroyed");
}

auto RenderWindow::GetInstance() -> RenderWindow& {
    static RenderWindow instance;
    return instance;
}

namespace {

} // namespace

auto RenderWindow::PumpEvents(const EventSink& sink) -> void {
    if (!backend_)
        return;
    auto& window = backend_->Window();

    while (auto event = window.PollEvent()) {
        if (event->type == atom::window::EventType::None)
            continue;

        if (event->type == atom::window::EventType::Resized) {
            const auto& resize = std::get<atom::window::ResizeEvent>(event->data);
            // Backend first: a consumer receiving the event observes the new
            // device dimensions, never a stale swapchain.
            backend_->Device().HandleResize(resize.width, resize.height);
            pending_resize_ = PendingResize{resize.width, resize.height, window.GetTimeSeconds()};
        }

        if (sink)
            sink(*event);

        if (!window.IsOpen())
            break;
    }

    if (!window.IsOpen() || !pending_resize_)
        return;
    const double now = window.GetTimeSeconds();
    if (now - pending_resize_->last_change_seconds < resize_settle_delay_seconds)
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
    if (sink)
        sink(settledEvent);
}

auto RenderWindow::Initialize(const atom::backend::RenderBackendConfig& config) -> Result<void> {
    // The runtime layer owns concrete backends; this facade only consumes the
    // registry and the IRenderBackend/IWindow/IRenderDevice contracts.
    atom::backend::RenderBackendRuntime::GetInstance().EnsureDefaultRenderBackend();
    const auto backend_id = atom::backend::ToString(config.backend);
    name_ = config.title;
    backend_id_ = std::string{backend_id};

    shutdown_notified_ = false;
    pending_resize_.reset();

    auto& registry = atom::backend::RenderBackendRegistry::GetInstance();
    backend_ = registry.CreateBackend(backend_id);
    if (!backend_) {
        // The caller gets the reason; the log line stays because a startup
        // failure is also useful in a crash/telemetry dump.
        auto error = Error{ErrorCode::RenderBackendUnavailable, Module::Window, "RenderWindow::Initialize"}
                         .WithDetail("render backend '" + backend_id_ + "' is not registered");
        LOG_ERROR(atom::log::core::Window, error.Message());
        return std::unexpected<Error>{std::move(error)};
    }

    if (!backend_->Initialize(config)) {
        backend_.reset();
        auto error = Error{ErrorCode::RenderDeviceCreationFailed, Module::Window, "RenderWindow::Initialize"}
                         .WithDetail("render backend '" + backend_id_ + "' failed to initialize");
        LOG_ERROR(atom::log::core::Window, error.Message());
        return std::unexpected<Error>{std::move(error)};
    }
    backend_->Window().SetFPS(fps_);

    return Ok();
}

auto RenderWindow::BeginFrame() -> bool {
    if (!backend_)
        return false;
    return backend_->Device().BeginFrame();
}

auto RenderWindow::EndFrame() -> void {
    if (backend_)
        backend_->Device().EndFrame();
}

auto RenderWindow::WaitForNextFrame() -> void {
    if (backend_)
        backend_->Window().WaitForNextFrame();
}

auto RenderWindow::SetFPS(const unsigned int fps) -> void {
    fps_ = fps;
    if (backend_)
        backend_->Window().SetFPS(fps);
}

auto RenderWindow::SetTextInputEnabled(const bool enabled) -> void {
    if (backend_)
        backend_->Window().SetTextInputEnabled(enabled);
}

auto RenderWindow::IsTextInputEnabled() const -> bool {
    return backend_ ? backend_->Window().IsTextInputEnabled() : false;
}

auto RenderWindow::GetIWindow() -> atom::window::IWindow* {
    return backend_ ? &backend_->Window() : nullptr;
}
auto RenderWindow::GetRenderDevice() -> atom::render::IRenderDevice* {
    return backend_ ? &backend_->Device() : nullptr;
}

auto RenderWindow::GetTimeSource() -> atom::backend::ITimeSource* {
    return backend_ ? &backend_->TimeSource() : nullptr;
}

auto RenderWindow::GetFPS() const -> unsigned {
    return fps_;
}

auto RenderWindow::IsOpen() const -> bool {
    return backend_ && backend_->Window().IsOpen();
}

auto RenderWindow::Shutdown() -> void {
    if (shutdown_notified_)
        return;
    shutdown_notified_ = true;
    LOG_INFO(atom::log::core::Window, "Render window shutting down: " + name_);
    if (backend_) {
        backend_->Shutdown();
        backend_.reset();
    }
    pending_resize_.reset();
}

auto RenderWindow::RequestClose() -> void {
    if (backend_) {
        backend_->Window().RequestClose();
    }
}

auto RenderWindow::GetName() const -> const std::string& {
    return name_;
}

auto RenderWindow::GetBackendId() const -> const std::string& {
    return backend_id_;
}

} // namespace atom

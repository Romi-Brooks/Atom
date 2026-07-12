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

namespace atom {

auto RenderWindow::GetInstance() -> RenderWindow& {
    static RenderWindow instance;
    return instance;
}

auto RenderWindow::ProcessEvents(const ScreenManager& screenManager) -> void {
    while (auto event = window_->PollEvent()) {
        if (on_process_event_) {
            on_process_event_(*event);
        }

        screenManager.HandleEvent(*event);

        if (!window_->IsOpen()) break;

        if (event->type == EventType::Closed) {
            window_->Shutdown();
            break;
        }
    }
}

auto RenderWindow::Initialize(const std::string& title, Vec2 resolution) -> void {
    window_ = std::make_unique<SDL3RenderWindow>();
    window_->Initialize(title, resolution);

    window_->on_pre_process_sdl_event_ = [this](const SDL_Event& ev) {
        if (on_pre_process_sdl_event_) on_pre_process_sdl_event_(ev);
    };
    window_->on_process_event_ = [this](IEvent& event) {
        if (on_process_event_) on_process_event_(event);
    };
    window_->on_update_ = [this](float dt) {
        if (on_update_) on_update_(dt);
    };
    window_->on_render_overlay_ = [this]() {
        if (on_render_overlay_) on_render_overlay_();
    };
    window_->on_shutdown_ = [this]() {
        if (on_shutdown_) on_shutdown_();
    };
}

auto RenderWindow::Run() -> void {
    const auto& screenManager = atom::ScreenManager::GetInstance();
    if (screenManager.GetCurrentScreenName().empty()) {
        throw std::runtime_error("No current screen set. Cannot run application.");
    }

    const auto frameDuration = std::chrono::duration<float, std::milli>(1000.0f / static_cast<float>(fps_));
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (window_->IsOpen()) {
        const auto frameStart = std::chrono::high_resolution_clock::now();

        ProcessEvents(screenManager);

        if (!window_->IsOpen()) break;

        const float deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        if (window_->on_update_) {
            window_->on_update_(deltaTime);
        }

        window_->Clear(atom::Color::Black());
        screenManager.Render(*window_);

        if (window_->on_render_overlay_) {
            window_->on_render_overlay_();
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
    if (window_) window_->SetFPS(fps);
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
    if (window_) {
        window_->Shutdown();
    }
}

auto RenderWindow::GetNativeWindowHandle() const -> void* {
    return window_ ? window_->GetNativeWindowHandle() : nullptr;
}

auto RenderWindow::GetNativeRendererHandle() const -> void* {
    return window_ ? window_->GetNativeRendererHandle() : nullptr;
}

} // namespace atom

/**
  * @file           : RenderWindow.hpp
  * @author         : Romi Brooks
  * @brief          : Main render window singleton (Engine Core)
  * @attention      : Wraps an IRenderWindow (SDL3 backend) behind a stable singleton API.
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_RENDER_WINDOW_HPP
#define ATOM_RENDER_WINDOW_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include <Engine/Interfaces/IRenderWindow.hpp>
#include <Engine/Render/SDL3RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>

namespace atom {

class RenderWindow {
    private:
        std::unique_ptr<SDL3RenderWindow> window_;
        unsigned int fps_ = 60;

        RenderWindow() = default;
        ~RenderWindow() = default;

        auto ProcessEvents(const ScreenManager& screenManager) -> void {
            while (auto event = window_->PollEvent()) {
                // Optional overlay event processing (post-translation)
                // 可选的叠加层事件处理
                if (on_process_event_) {
                    on_process_event_(*event);
                }

                screenManager.HandleEvent(*event);

                // If HandleEvent triggered shutdown (e.g., ESC key), stop processing
                if (!window_->IsOpen()) break;

                if (event->type == EventType::Closed) {
                    window_->Shutdown();
                    break;
                }
            }
        }

    public:
        [[nodiscard]] static auto GetInstance() -> RenderWindow& {
            static RenderWindow instance;
            return instance;
        }

        // --- Callback hooks for optional debug overlays ---
        //     可选的调试叠加层回调钩子

        // Called per raw SDL event before translation (for ImGui platform processing)
        // 每原始SDL事件调用一次，翻译前（ImGui平台处理）
        std::function<void(const SDL_Event&)> on_pre_process_sdl_event_;

        // Called per event for overlay event processing (after translation)
        // 每事件调用一次，用于叠加层事件处理（翻译后）
        std::function<void(IEvent&)> on_process_event_;

        // Called per frame before rendering for overlay update
        // 每帧渲染前调用，用于叠加层更新
        std::function<void(float deltaTime)> on_update_;

        // Called per frame after scene render for overlay rendering
        // 每帧场景渲染后调用，用于叠加层渲染
        std::function<void()> on_render_overlay_;

        // Called on shutdown for overlay cleanup
        // 关闭时调用，用于叠加层清理
        std::function<void()> on_shutdown_;

        // Core API
        auto Initialize(const std::string& title, Vec2 resolution) -> void {
            window_ = std::make_unique<SDL3RenderWindow>();
            window_->Initialize(title, resolution);

            // Forward callbacks to the SDL3 backend
            // 将回调转发到SDL3后端
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

        auto Run() -> void {
            const auto& screenManager = atom::ScreenManager::GetInstance();
            if (screenManager.GetCurrentScreenName().empty()) {
                throw std::runtime_error("No current screen set. Cannot run application.");
            }

            const auto frameDuration = std::chrono::duration<float, std::milli>(1000.0f / static_cast<float>(fps_));
            auto lastTime = std::chrono::high_resolution_clock::now();

            while (window_->IsOpen()) {
                const auto frameStart = std::chrono::high_resolution_clock::now();

                // Event processing
                ProcessEvents(screenManager);

                if (!window_->IsOpen()) break;

                // Delta time
                const float deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
                lastTime = frameStart;

                // Optional overlay update (e.g., ImGui)
                if (window_->on_update_) {
                    window_->on_update_(deltaTime);
                }

                // Render scene
                window_->Clear(atom::Color::Black());
                screenManager.Render(*window_);

                // Optional overlay rendering (e.g., ImGui)
                if (window_->on_render_overlay_) {
                    window_->on_render_overlay_();
                }

                window_->Display();

                // Frame rate limiting
                const auto frameEnd = std::chrono::high_resolution_clock::now();
                const auto frameTime = std::chrono::duration<float, std::milli>(frameEnd - frameStart);
                if (frameTime < frameDuration) {
                    std::this_thread::sleep_for(frameDuration - frameTime);
                }
            }
            Shutdown();
        }

        auto SetFPS(const unsigned int fps) -> void {
            fps_ = fps;
            if (window_) window_->SetFPS(fps);
        }

        [[nodiscard]] auto GetIRenderWindow() -> IRenderWindow* {
            return window_.get();
        }

        [[nodiscard]] auto GetWindow() const -> const SDL3RenderWindow* {
            return window_.get();
        }

        [[nodiscard]] auto GetFPS() const -> unsigned {
            return fps_;
        }

        [[nodiscard]] auto IsOpen() const -> bool {
            return window_ && window_->IsOpen();
        }

        auto Shutdown() -> void {
            if (window_) {
                window_->Shutdown();
            }
        }

        // Access to native handles for ImGui interop
        [[nodiscard]] auto GetNativeWindowHandle() const -> void* {
            return window_ ? window_->GetNativeWindowHandle() : nullptr;
        }

        [[nodiscard]] auto GetNativeRendererHandle() const -> void* {
            return window_ ? window_->GetNativeRendererHandle() : nullptr;
        }
};

} // namespace atom

#endif // ATOM_RENDER_WINDOW_HPP

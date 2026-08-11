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

#include <functional>
#include <memory>
#include <string>

#include <Backend/Contracts/Render/IRenderWindow.hpp>
#include <Backend/SDL3/Window/SDL3RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>

namespace atom {

class RenderWindow {
    private:
        std::unique_ptr<SDL3RenderWindow> window_;
        unsigned int fps_ = 60;

        RenderWindow() = default;
        ~RenderWindow() = default;

        auto ProcessEvents(const ScreenManager& screenManager) -> void;

    public:
        RenderWindow(const RenderWindow&) = delete;
        auto operator=(const RenderWindow&) -> RenderWindow& = delete;

        [[nodiscard]] static auto GetInstance() -> RenderWindow&;

        // --- Callback hooks for optional debug overlays ---

        // Called per raw SDL event before translation (for ImGui platform processing)
        std::function<void(const SDL_Event&)> on_pre_process_sdl_event_;

        // Called per event for overlay event processing (after translation)
        std::function<void(IEvent&)> on_process_event_;

        // Called per frame before rendering for overlay update
        std::function<void(float deltaTime)> on_update_;

        // Called per frame after scene render for overlay rendering
        std::function<void()> on_render_overlay_;

        // Called on shutdown for overlay cleanup
        std::function<void()> on_shutdown_;

        // Core API
        auto Initialize(const std::string& title, Vec2 resolution) -> void;
        auto Run() -> void;
        auto SetFPS(unsigned int fps) -> void;
        [[nodiscard]] auto GetFPS() const -> unsigned;
        [[nodiscard]] auto IsOpen() const -> bool;
        auto Shutdown() -> void;

        // Backend access
        [[nodiscard]] auto GetIRenderWindow() -> IRenderWindow*;

        // Access to native handles for ImGui interop
        [[nodiscard]] auto GetNativeWindowHandle() const -> void*;
        [[nodiscard]] auto GetNativeRendererHandle() const -> void*;
};

} // namespace atom

#endif // ATOM_RENDER_WINDOW_HPP

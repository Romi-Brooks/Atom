/**
  * @file           : SDL3Window.hpp
  * @author         : Romi Brooks
  * @brief          : SDL3 implementation of the backend-neutral window API.
 * @attention      : Native SDL handles remain internal to the SDL3 backend.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDL3_WINDOW_HPP
#define ATOM_BACKEND_SDL3_WINDOW_HPP

#include <limits>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Time/ITimeSource.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>

namespace atom::backend::sdl3 {

class SDL3Window final : public window::IWindow {
    public:
        ~SDL3Window() override;
        auto Initialize(const std::string& title, algo::Vec2 resolution) -> bool override;
        auto Shutdown() -> void override;
        auto RequestClose() -> void override;
        [[nodiscard]] auto IsOpen() const -> bool override;
        [[nodiscard]] auto PollEvent() -> std::optional<window::IEvent> override;
        auto SetFPS(uint32_t fps) -> void override;
        [[nodiscard]] auto GetFPS() const -> uint32_t override;
        [[nodiscard]] auto GetTimeSeconds() const -> double override;
        auto WaitForNextFrame() -> void override;
        [[nodiscard]] auto GetSize() const -> algo::Vec2 override;
        auto SetTextInputEnabled(bool enabled) -> void override;
        [[nodiscard]] auto IsTextInputEnabled() const -> bool override;
        auto SetWindowMode(window::WindowMode mode) -> bool override;
        [[nodiscard]] auto GetWindowMode() const -> window::WindowMode override;
        [[nodiscard]] auto GetNativeWindow() const -> SDL_Window*;

        // Bind the backend's host clock. The window deliberately owns no counter
        // of its own: one ITimeSource keeps frame pacing and atom::time on the
        // same epoch (CORE-001).
        auto SetTimeSource(backend::ITimeSource& source) -> void;

    private:
        SDL_Window* window_ = nullptr;
        backend::ITimeSource* time_source_ = nullptr;
        uint32_t fps_limit_ = 60;
        uint64_t next_frame_ns_ = 0;
        // The configured window size, kept so leaving a fullscreen placement can
        // restore it instead of inheriting whatever the display mode left behind.
        int windowed_width_ = 0;
        int windowed_height_ = 0;
        // Placement currently applied to window_. The OS can yank the window out
        // of fullscreen on its own (alt-tab to a display change, Win+D), so this
        // is "what we last asked for", reconciled on the next SetWindowMode.
        window::WindowMode window_mode_ = window::WindowMode::Windowed;
        // Whether the OS input method is currently attached (see SetTextInputEnabled).
        bool text_input_enabled_ = false;
        bool open_ = false;
        SDLSubsystemLease video_runtime_;
        SDLSubsystemLease events_runtime_;
};

} // namespace atom::backend::sdl3

#endif // ATOM_BACKEND_SDL3_WINDOW_HPP

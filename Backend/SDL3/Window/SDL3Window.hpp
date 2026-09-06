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

#include <SDL3/SDL.h>

#include <Backend/Contracts/Window/IWindow.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>

namespace atom::backend::sdl3 {

class SDL3Window final : public window::IWindow {
    public:
        ~SDL3Window() override;
        auto Initialize(const std::string& title, algo::Vec2 resolution) -> bool override;
        auto Shutdown() -> void override;
        [[nodiscard]] auto IsOpen() const -> bool override;
        [[nodiscard]] auto PollEvent() -> std::optional<window::IEvent> override;
        auto SetFPS(uint32_t fps) -> void override;
        [[nodiscard]] auto GetFPS() const -> uint32_t override;
        [[nodiscard]] auto GetTimeSeconds() const -> double override;
        auto WaitForNextFrame() -> void override;
        [[nodiscard]] auto GetSize() const -> algo::Vec2 override;
        [[nodiscard]] auto GetNativeWindow() const -> SDL_Window*;

    private:
        SDL_Window* window_ = nullptr;
        uint32_t fps_limit_ = 60;
        uint64_t performance_frequency_ = 0;
        uint64_t next_frame_counter_ = 0;
        bool open_ = false;
        SDLSubsystemLease video_runtime_;
        SDLSubsystemLease events_runtime_;
};

} // namespace atom::backend::sdl3

#endif // ATOM_BACKEND_SDL3_WINDOW_HPP

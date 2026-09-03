/**
  * @file           : SDL3Window.hpp
  * @author         : Romi Brooks
  * @brief          : SDL3 implementation of the backend-neutral window API.
  * @attention      : Native SDL handles are exposed only through SDL3 extensions.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDL3_WINDOW_HPP
#define ATOM_BACKEND_SDL3_WINDOW_HPP

#include <functional>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Window/IWindow.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>
#include <Backend/SDL3/Window/ISDL3WindowExtensions.hpp>

namespace atom::backend::sdl3 {

class SDL3Window final : public window::IWindow, public ISDL3WindowExtensions {
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
        auto SetRawEventHook(std::function<void(const void*)> hook) -> void override;
        [[nodiscard]] auto GetSize() const -> algo::Vec2 override;
        [[nodiscard]] auto GetNativeWindow() const -> SDL_Window* override;

    private:
        SDL_Window* window_ = nullptr;
        std::function<void(const void*)> raw_event_hook_;
        uint32_t fps_limit_ = 60;
        uint64_t performance_frequency_ = 0;
        uint64_t next_frame_counter_ = 0;
        bool open_ = false;
        SDLSubsystemLease video_runtime_;
        SDLSubsystemLease events_runtime_;
};

} // namespace atom::backend::sdl3

#endif

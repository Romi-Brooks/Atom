/**
  * @file           : SDL3Window.cpp
  * @author         : Romi Brooks
  * @brief          : Implements the SDL3 window and event adapter.
  * @attention      : Native SDL handles stay inside the SDL3 backend boundary.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDL3Window.hpp"

#include <algorithm>
#include <utility>

#include <Log/LogSystem.hpp>

namespace atom::backend::sdl3 {

namespace {
auto TranslateEvent(const SDL_Event& event) -> window::IEvent {
    window::IEvent result{};
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        result.type = window::EventType::Closed;
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        result.type = event.type == SDL_EVENT_KEY_DOWN ? window::EventType::KeyPressed : window::EventType::KeyReleased;
        result.data = window::KeyEvent{static_cast<int32_t>(event.key.scancode), static_cast<int32_t>(event.key.key),
                                       bool(event.key.mod & SDL_KMOD_ALT), bool(event.key.mod & SDL_KMOD_CTRL),
                                       bool(event.key.mod & SDL_KMOD_SHIFT)};
        break;
    case SDL_EVENT_MOUSE_MOTION:
        result.type = window::EventType::MouseMoved;
        result.data = window::MouseEvent{event.motion.x, event.motion.y, 0};
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        result.type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? window::EventType::MouseButtonPressed
                                                                : window::EventType::MouseButtonReleased;
        result.data = window::MouseEvent{event.button.x, event.button.y, static_cast<int32_t>(event.button.button)};
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        result.type = window::EventType::Resized;
        result.data =
            window::ResizeEvent{static_cast<uint32_t>(event.window.data1), static_cast<uint32_t>(event.window.data2)};
        break;
    default:
        break;
    }
    return result;
}
} // namespace

SDL3Window::~SDL3Window() {
    Shutdown();
}

auto SDL3Window::Initialize(const std::string& title, const algo::Vec2 resolution) -> bool {
    SDLSubsystemLease video(SDLSubsystem::Video);
    SDLSubsystemLease events(SDLSubsystem::Events);
    if (!video.IsValid() || !events.IsValid())
        return false;
    window_ = SDL_CreateWindow(title.c_str(), static_cast<int>(resolution.GetX()), static_cast<int>(resolution.GetY()),
                               SDL_WINDOW_RESIZABLE);
    if (!window_) {
        LOG_ERROR(LogChannel::WINDOW, "SDL_CreateWindow failed: " + std::string{SDL_GetError()});
        return false;
    }
    performance_frequency_ = SDL_GetPerformanceFrequency();
    video_runtime_ = std::move(video);
    events_runtime_ = std::move(events);
    open_ = true;
    return true;
}

auto SDL3Window::Shutdown() -> void {
    if (window_)
        SDL_DestroyWindow(window_);
    window_ = nullptr;
    open_ = false;
    events_runtime_.Reset();
    video_runtime_.Reset();
}
auto SDL3Window::IsOpen() const -> bool {
    return open_;
}
auto SDL3Window::PollEvent() -> std::optional<window::IEvent> {
    SDL_Event event;
    if (!SDL_PollEvent(&event))
        return std::nullopt;
    if (raw_event_hook_)
        raw_event_hook_(&event);
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        open_ = false;
    return TranslateEvent(event);
}
auto SDL3Window::SetFPS(const uint32_t fps) -> void {
    fps_limit_ = fps;
    next_frame_counter_ = 0;
}
auto SDL3Window::GetFPS() const -> uint32_t {
    return fps_limit_;
}
auto SDL3Window::GetTimeSeconds() const -> double {
    return performance_frequency_ == 0 ? 0.0
                                       : static_cast<double>(SDL_GetPerformanceCounter()) / performance_frequency_;
}
auto SDL3Window::WaitForNextFrame() -> void {
    if (fps_limit_ == 0 || performance_frequency_ == 0)
        return;
    const auto span = std::max<uint64_t>(1, performance_frequency_ / fps_limit_);
    auto now = SDL_GetPerformanceCounter();
    if (next_frame_counter_ == 0)
        next_frame_counter_ = now + span;
    if (now < next_frame_counter_) {
        SDL_DelayPrecise(static_cast<uint64_t>(static_cast<long double>(next_frame_counter_ - now) * SDL_NS_PER_SECOND /
                                               performance_frequency_));
        now = SDL_GetPerformanceCounter();
    }
    next_frame_counter_ += span;
    if (now > next_frame_counter_ + span)
        next_frame_counter_ = now + span;
}
auto SDL3Window::SetRawEventHook(std::function<void(const void*)> hook) -> void {
    raw_event_hook_ = std::move(hook);
}
auto SDL3Window::GetSize() const -> algo::Vec2 {
    int width = 0, height = 0;
    if (window_)
        SDL_GetWindowSizeInPixels(window_, &width, &height);
    return {static_cast<float>(width), static_cast<float>(height)};
}
auto SDL3Window::GetNativeWindow() const -> SDL_Window* {
    return window_;
}

} // namespace atom::backend::sdl3

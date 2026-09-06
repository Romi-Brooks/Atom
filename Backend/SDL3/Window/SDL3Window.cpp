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
auto TranslateKey(const SDL_Scancode scancode) -> event::Key {
    using event::Key;
    switch (scancode) {
    case SDL_SCANCODE_A:
        return Key::A;
    case SDL_SCANCODE_B:
        return Key::B;
    case SDL_SCANCODE_C:
        return Key::C;
    case SDL_SCANCODE_D:
        return Key::D;
    case SDL_SCANCODE_E:
        return Key::E;
    case SDL_SCANCODE_F:
        return Key::F;
    case SDL_SCANCODE_G:
        return Key::G;
    case SDL_SCANCODE_H:
        return Key::H;
    case SDL_SCANCODE_I:
        return Key::I;
    case SDL_SCANCODE_J:
        return Key::J;
    case SDL_SCANCODE_K:
        return Key::K;
    case SDL_SCANCODE_L:
        return Key::L;
    case SDL_SCANCODE_M:
        return Key::M;
    case SDL_SCANCODE_N:
        return Key::N;
    case SDL_SCANCODE_O:
        return Key::O;
    case SDL_SCANCODE_P:
        return Key::P;
    case SDL_SCANCODE_Q:
        return Key::Q;
    case SDL_SCANCODE_R:
        return Key::R;
    case SDL_SCANCODE_S:
        return Key::S;
    case SDL_SCANCODE_T:
        return Key::T;
    case SDL_SCANCODE_U:
        return Key::U;
    case SDL_SCANCODE_V:
        return Key::V;
    case SDL_SCANCODE_W:
        return Key::W;
    case SDL_SCANCODE_X:
        return Key::X;
    case SDL_SCANCODE_Y:
        return Key::Y;
    case SDL_SCANCODE_Z:
        return Key::Z;
    case SDL_SCANCODE_0:
        return Key::Digit0;
    case SDL_SCANCODE_1:
        return Key::Digit1;
    case SDL_SCANCODE_2:
        return Key::Digit2;
    case SDL_SCANCODE_3:
        return Key::Digit3;
    case SDL_SCANCODE_4:
        return Key::Digit4;
    case SDL_SCANCODE_5:
        return Key::Digit5;
    case SDL_SCANCODE_6:
        return Key::Digit6;
    case SDL_SCANCODE_7:
        return Key::Digit7;
    case SDL_SCANCODE_8:
        return Key::Digit8;
    case SDL_SCANCODE_9:
        return Key::Digit9;
    case SDL_SCANCODE_ESCAPE:
        return Key::Escape;
    case SDL_SCANCODE_RETURN:
        return Key::Enter;
    case SDL_SCANCODE_TAB:
        return Key::Tab;
    case SDL_SCANCODE_BACKSPACE:
        return Key::Backspace;
    case SDL_SCANCODE_SPACE:
        return Key::Space;
    case SDL_SCANCODE_INSERT:
        return Key::Insert;
    case SDL_SCANCODE_DELETE:
        return Key::Delete;
    case SDL_SCANCODE_HOME:
        return Key::Home;
    case SDL_SCANCODE_END:
        return Key::End;
    case SDL_SCANCODE_PAGEUP:
        return Key::PageUp;
    case SDL_SCANCODE_PAGEDOWN:
        return Key::PageDown;
    case SDL_SCANCODE_LEFT:
        return Key::Left;
    case SDL_SCANCODE_RIGHT:
        return Key::Right;
    case SDL_SCANCODE_UP:
        return Key::Up;
    case SDL_SCANCODE_DOWN:
        return Key::Down;
    case SDL_SCANCODE_F1:
        return Key::F1;
    case SDL_SCANCODE_F2:
        return Key::F2;
    case SDL_SCANCODE_F3:
        return Key::F3;
    case SDL_SCANCODE_F4:
        return Key::F4;
    case SDL_SCANCODE_F5:
        return Key::F5;
    case SDL_SCANCODE_F6:
        return Key::F6;
    case SDL_SCANCODE_F7:
        return Key::F7;
    case SDL_SCANCODE_F8:
        return Key::F8;
    case SDL_SCANCODE_F9:
        return Key::F9;
    case SDL_SCANCODE_F10:
        return Key::F10;
    case SDL_SCANCODE_F11:
        return Key::F11;
    case SDL_SCANCODE_F12:
        return Key::F12;
    case SDL_SCANCODE_LSHIFT:
        return Key::LeftShift;
    case SDL_SCANCODE_RSHIFT:
        return Key::RightShift;
    case SDL_SCANCODE_LCTRL:
        return Key::LeftControl;
    case SDL_SCANCODE_RCTRL:
        return Key::RightControl;
    case SDL_SCANCODE_LALT:
        return Key::LeftAlt;
    case SDL_SCANCODE_RALT:
        return Key::RightAlt;
    case SDL_SCANCODE_LGUI:
        return Key::LeftSuper;
    case SDL_SCANCODE_RGUI:
        return Key::RightSuper;
    case SDL_SCANCODE_CAPSLOCK:
        return Key::CapsLock;
    default:
        return Key::Unknown;
    }
}

auto TranslateModifiers(const SDL_Keymod modifiers) -> event::KeyModifiers {
    auto result = event::KeyModifiers::None;
    if (modifiers & SDL_KMOD_ALT)
        result = result | event::KeyModifier::Alt;
    if (modifiers & SDL_KMOD_CTRL)
        result = result | event::KeyModifier::Control;
    if (modifiers & SDL_KMOD_SHIFT)
        result = result | event::KeyModifier::Shift;
    if (modifiers & SDL_KMOD_GUI)
        result = result | event::KeyModifier::Super;
    return result;
}

auto TranslateMouseButton(const uint8_t button) -> event::MouseButton {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return event::MouseButton::Left;
    case SDL_BUTTON_MIDDLE:
        return event::MouseButton::Middle;
    case SDL_BUTTON_RIGHT:
        return event::MouseButton::Right;
    case SDL_BUTTON_X1:
        return event::MouseButton::X1;
    case SDL_BUTTON_X2:
        return event::MouseButton::X2;
    default:
        return event::MouseButton::Unknown;
    }
}

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
        result.data =
            window::KeyEvent{TranslateKey(event.key.scancode), TranslateModifiers(event.key.mod), event.key.repeat};
        break;
    case SDL_EVENT_MOUSE_MOTION:
        result.type = window::EventType::MouseMoved;
        result.data = window::MouseEvent{event.motion.x, event.motion.y, event::MouseButton::Unknown};
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        result.type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? window::EventType::MouseButtonPressed
                                                                : window::EventType::MouseButtonReleased;
        result.data = window::MouseEvent{event.button.x, event.button.y, TranslateMouseButton(event.button.button)};
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        result.type = window::EventType::MouseWheel;
        result.data = window::MouseWheelEvent{event.wheel.x, event.wheel.y};
        break;
    case SDL_EVENT_TEXT_INPUT:
        result.type = window::EventType::TextInput;
        result.data = window::TextInputEvent{event.text.text};
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        result.type = window::EventType::FocusChanged;
        result.data = window::FocusEvent{event.type == SDL_EVENT_WINDOW_FOCUS_GAINED};
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

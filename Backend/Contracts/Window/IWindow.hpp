/**
  * @file           : IWindow.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral window, event and native-handle contract.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_WINDOW_IWINDOW_HPP
#define ATOM_BACKEND_CONTRACTS_WINDOW_IWINDOW_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::window {

enum class EventType {
    None = 0,
    Closed,
    KeyPressed,
    KeyReleased,
    MouseMoved,
    MouseButtonPressed,
    MouseButtonReleased,
    Resized
};

struct KeyEvent {
        int32_t scancode = 0, keycode = 0;
        bool alt = false, ctrl = false, shift = false;
};
struct MouseEvent {
        float x = 0, y = 0;
        int32_t button = 0;
};
struct ResizeEvent {
        uint32_t width = 0, height = 0;
};
struct IEvent {
        EventType type = EventType::None;
        std::variant<KeyEvent, MouseEvent, ResizeEvent> data{};
};

class IWindow {
    public:
        virtual ~IWindow() = default;
        virtual auto Initialize(const std::string& title, algo::Vec2 resolution) -> bool = 0;
        virtual auto Shutdown() -> void = 0;
        [[nodiscard]] virtual auto IsOpen() const -> bool = 0;
        [[nodiscard]] virtual auto PollEvent() -> std::optional<IEvent> = 0;
        virtual auto SetFPS(uint32_t fps) -> void = 0;
        [[nodiscard]] virtual auto GetFPS() const -> uint32_t = 0;
        [[nodiscard]] virtual auto GetTimeSeconds() const -> double = 0;
        virtual auto WaitForNextFrame() -> void = 0;
        virtual auto SetRawEventHook(std::function<void(const void*)> hook) -> void = 0;
        [[nodiscard]] virtual auto GetSize() const -> algo::Vec2 = 0;
};

} // namespace atom::window

#endif

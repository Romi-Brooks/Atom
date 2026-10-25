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
#include <optional>
#include <string>
#include <variant>

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Window/WindowMode.hpp>
#include <Input/Key.hpp>

namespace atom::window {

enum class EventType {
    None = 0,
    Closed,
    KeyPressed,
    KeyReleased,
    MouseMoved,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseWheel,
    TextInput,
    FocusChanged,
    Resized,
    // Synthesized by the engine after resize notifications have been quiet
    // for its configured debounce interval; never emitted by an OS backend.
    ResizeSettled
};

// Input payloads are declared by Input, rather than by a window backend.
// Aliases preserve the window event API while keeping key identity portable.
using KeyEvent = input::KeyEvent;
struct MouseEvent {
        float x = 0, y = 0;
        input::MouseButton button = input::MouseButton::Unknown;
};
struct MouseWheelEvent {
        float x = 0, y = 0;
};
struct TextInputEvent {
        std::string text;
};
struct FocusEvent {
        bool focused = false;
};
struct ResizeEvent {
        uint32_t width = 0, height = 0;
};
struct IEvent {
        EventType type = EventType::None;
        std::variant<KeyEvent, MouseEvent, MouseWheelEvent, TextInputEvent, FocusEvent, ResizeEvent> data{};
};

class IWindow {
    public:
        virtual ~IWindow() = default;
        virtual auto Initialize(const std::string& title, algo::Vec2 resolution) -> bool = 0;
        virtual auto Shutdown() -> void = 0;
        // Requests the window to close: only marks IsOpen() false, releasing no
        // resources. Screens call this from event handling to request exit; the
        // actual teardown happens later in Run()'s Shutdown(), so a screen never
        // destroys itself from inside its own event callback.
        virtual auto RequestClose() -> void = 0;
        [[nodiscard]] virtual auto IsOpen() const -> bool = 0;
        [[nodiscard]] virtual auto PollEvent() -> std::optional<IEvent> = 0;
        virtual auto SetFPS(uint32_t fps) -> void = 0;
        [[nodiscard]] virtual auto GetFPS() const -> uint32_t = 0;
        [[nodiscard]] virtual auto GetTimeSeconds() const -> double = 0;
        virtual auto WaitForNextFrame() -> void = 0;
        [[nodiscard]] virtual auto GetSize() const -> algo::Vec2 = 0;

        // Window placement. SetWindowMode() may change the display mode and
        // therefore the size reported by GetSize(), so callers must treat it as
        // a resize: never call it between BeginFrame() and EndFrame() of the
        // render device, and reconcile cached sizes afterwards.
        //
        // Returns false when the backend cannot provide that placement; the
        // mode is then left unchanged and GetWindowMode() still reports the
        // current one. It does not silently substitute a different mode.
        virtual auto SetWindowMode(WindowMode mode) -> bool = 0;
        [[nodiscard]] virtual auto GetWindowMode() const -> WindowMode = 0;
};

} // namespace atom::window

#endif // ATOM_BACKEND_CONTRACTS_WINDOW_IWINDOW_HPP

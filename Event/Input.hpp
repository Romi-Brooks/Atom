/**
 * @file           : Input.hpp
 * @author         : Romi Brooks
 * @brief          : Backend-neutral input events and keyboard binding primitives.
 * @attention      : Key represents a physical key position. Text input belongs
 *                   to a separate text-input event stream.
 * @date           : 2026/9/7
 * Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_EVENT_INPUT_HPP
#define ATOM_EVENT_INPUT_HPP

#include <cstdint>

namespace atom::event {

// A physical key, normalized by a platform backend. Keeping this independent
// of character input makes shortcuts and game controls keyboard-layout stable.
enum class Key : uint16_t {
    Unknown = 0,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Escape,
    Enter,
    Tab,
    Backspace,
    Space,
    Insert,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,
    Left,
    Right,
    Up,
    Down,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    LeftSuper,
    RightSuper,
    CapsLock
};

enum class KeyModifier : uint8_t { None = 0, Alt = 1 << 0, Control = 1 << 1, Shift = 1 << 2, Super = 1 << 3 };

using KeyModifiers = KeyModifier;

constexpr auto operator|(const KeyModifier left, const KeyModifier right) -> KeyModifiers {
    return static_cast<KeyModifiers>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
}

constexpr auto operator&(const KeyModifier left, const KeyModifier right) -> KeyModifiers {
    return static_cast<KeyModifiers>(static_cast<uint8_t>(left) & static_cast<uint8_t>(right));
}

constexpr auto HasModifier(const KeyModifiers modifiers, const KeyModifier modifier) -> bool {
    return (modifiers & modifier) == modifier;
}

struct KeyEvent {
        Key key = Key::Unknown;
        KeyModifiers modifiers = KeyModifiers::None;
        bool is_repeat = false;
};

enum class MouseButton : uint8_t { Unknown = 0, Left, Middle, Right, X1, X2 };

} // namespace atom::event

#endif // ATOM_EVENT_INPUT_HPP

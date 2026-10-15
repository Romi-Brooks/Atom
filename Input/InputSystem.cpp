/**
 * @file           : Input/InputSystem.cpp
 * @brief          : Implementation of the per-frame input state machine.
 */

#include "InputSystem.hpp"

#include <Backend/Contracts/Window/IWindow.hpp>

#include <variant>

namespace atom::input {

namespace {

constexpr auto Index(const Key key) -> std::size_t {
    return static_cast<std::size_t>(key);
}

constexpr auto Index(const MouseButton button) -> std::size_t {
    return static_cast<std::size_t>(button);
}

constexpr auto IsDown(const KeyState state) -> bool {
    return state == KeyState::Pressed || state == KeyState::Held;
}

} // namespace

auto InputSystem::GetInstance() -> InputSystem& {
    static InputSystem instance;
    return instance;
}

auto InputSystem::BeginFrame() -> void {
    // Edges last exactly one frame: Pressed settles into Held, Released into Up.
    for (auto& state : keys_) {
        if (state == KeyState::Pressed)
            state = KeyState::Held;
        else if (state == KeyState::Released)
            state = KeyState::Up;
    }
    for (auto& state : mouse_buttons_) {
        if (state == KeyState::Pressed)
            state = KeyState::Held;
        else if (state == KeyState::Released)
            state = KeyState::Up;
    }
    mouse_delta_ = algo::Vec2::Zero();
    mouse_wheel_ = algo::Vec2::Zero();
}

auto InputSystem::Reset() -> void {
    keys_.fill(KeyState::Up);
    mouse_buttons_.fill(KeyState::Up);
    mouse_delta_ = algo::Vec2::Zero();
    mouse_wheel_ = algo::Vec2::Zero();
    // Capture is owned by the UI layer and re-reported every frame, so clearing
    // it here only drops a stale claim; the next overlay callback restores it.
    ui_mouse_captured_ = false;
    ui_keyboard_captured_ = false;
    ui_text_input_captured_ = false;
    ui_capture_source_ = {};
}

auto InputSystem::ResetMousePosition() -> void {
    mouse_position_ = algo::Vec2::Zero();
    mouse_delta_ = algo::Vec2::Zero();
    has_mouse_position_ = false;
}

auto InputSystem::SetUiCapture(const bool mouse, const bool keyboard, const bool text_input,
                               const std::string_view source) -> void {
    ui_mouse_captured_ = mouse;
    ui_keyboard_captured_ = keyboard;
    ui_text_input_captured_ = text_input;
    // Keep the source only while something is actually captured, so a stale
    // name cannot outlive the claim that produced it.
    ui_capture_source_ = (mouse || keyboard || text_input) ? source : std::string_view{};
}

auto InputSystem::IsMouseCapturedByUi() const -> bool {
    return ui_mouse_captured_;
}

auto InputSystem::IsKeyboardCapturedByUi() const -> bool {
    return ui_keyboard_captured_;
}

auto InputSystem::IsTextInputCapturedByUi() const -> bool {
    return ui_text_input_captured_;
}

auto InputSystem::UiCaptureSource() const -> std::string_view {
    return ui_capture_source_;
}

auto InputSystem::SetDown(KeyState& state) -> void {
    // Up -> Pressed (edge). Released -> Pressed (re-pressed within one frame).
    // Pressed and Held are left untouched: a repeat arrives as Held after
    // BeginFrame() settled it, and re-raising the edge would break
    // IsKeyPressed for anything that acts once per press.
    if (state == KeyState::Up || state == KeyState::Released)
        state = KeyState::Pressed;
}

auto InputSystem::SetUp(KeyState& state) -> void {
    if (state == KeyState::Pressed || state == KeyState::Held)
        state = KeyState::Released;
}

auto InputSystem::ProcessEvent(const window::IEvent& event) -> void {
    switch (event.type) {
        case window::EventType::KeyPressed:
            if (const auto* key = std::get_if<window::KeyEvent>(&event.data)) {
                // An unmapped scancode translates to Key::Unknown, which is
                // index 0. Folding it in would silently press whatever key
                // happens to sit at index 0, so unknown keys are dropped.
                if (!key->is_repeat && key->key != Key::Unknown)
                    SetDown(keys_[Index(key->key)]);
            }
            break;
        case window::EventType::KeyReleased:
            if (const auto* key = std::get_if<window::KeyEvent>(&event.data)) {
                if (key->key != Key::Unknown)
                    SetUp(keys_[Index(key->key)]);
            }
            break;
        case window::EventType::MouseMoved:
            if (const auto* mouse = std::get_if<window::MouseEvent>(&event.data)) {
                const algo::Vec2 position{mouse->x, mouse->y};
                // The first observed position only establishes a baseline; it
                // must not fabricate a large delta from an unknown origin.
                if (has_mouse_position_)
                    mouse_delta_ += position - mouse_position_;
                mouse_position_ = position;
                has_mouse_position_ = true;
            }
            break;
        case window::EventType::MouseButtonPressed:
            if (const auto* mouse = std::get_if<window::MouseEvent>(&event.data)) {
                if (mouse->button != MouseButton::Unknown)
                    SetDown(mouse_buttons_[Index(mouse->button)]);
            }
            break;
        case window::EventType::MouseButtonReleased:
            if (const auto* mouse = std::get_if<window::MouseEvent>(&event.data)) {
                if (mouse->button != MouseButton::Unknown)
                    SetUp(mouse_buttons_[Index(mouse->button)]);
            }
            break;
        case window::EventType::MouseWheel:
            if (const auto* wheel = std::get_if<window::MouseWheelEvent>(&event.data))
                mouse_wheel_ += algo::Vec2{wheel->x, wheel->y};
            break;
        case window::EventType::FocusChanged:
            if (const auto* focus = std::get_if<window::FocusEvent>(&event.data); focus != nullptr && !focus->focused) {
                // The OS stops delivering key-up events once the window loses
                // focus, so every key pressed at that moment would stay "held"
                // forever and the game would drift until the window is focused
                // again. Dropping the state turns that into a clean release.
                Reset();
            }
            break;
        default:
            break;
    }
}

auto InputSystem::IsKeyPressed(const Key key) const -> bool {
    return keys_[Index(key)] == KeyState::Pressed;
}

auto InputSystem::IsKeyHeld(const Key key) const -> bool {
    return IsDown(keys_[Index(key)]);
}

auto InputSystem::IsKeyReleased(const Key key) const -> bool {
    return keys_[Index(key)] == KeyState::Released;
}

auto InputSystem::GetKeyState(const Key key) const -> KeyState {
    return keys_[Index(key)];
}

auto InputSystem::GetMousePosition() const -> algo::Vec2 {
    return mouse_position_;
}

auto InputSystem::GetMouseDelta() const -> algo::Vec2 {
    return mouse_delta_;
}

auto InputSystem::GetMouseWheel() const -> algo::Vec2 {
    return mouse_wheel_;
}

auto InputSystem::IsMousePressed(const MouseButton button) const -> bool {
    return mouse_buttons_[Index(button)] == KeyState::Pressed;
}

auto InputSystem::IsMouseHeld(const MouseButton button) const -> bool {
    return IsDown(mouse_buttons_[Index(button)]);
}

auto InputSystem::IsMouseReleased(const MouseButton button) const -> bool {
    return mouse_buttons_[Index(button)] == KeyState::Released;
}

auto InputSystem::ModifierHeld(const KeyModifiers modifiers) const -> bool {
    const auto held = [this](const Key key) { return IsKeyHeld(key); };
    if (HasModifier(modifiers, KeyModifiers::Shift) && !(held(Key::LeftShift) || held(Key::RightShift)))
        return false;
    if (HasModifier(modifiers, KeyModifiers::Control) && !(held(Key::LeftControl) || held(Key::RightControl)))
        return false;
    if (HasModifier(modifiers, KeyModifiers::Alt) && !(held(Key::LeftAlt) || held(Key::RightAlt)))
        return false;
    if (HasModifier(modifiers, KeyModifiers::Super) && !(held(Key::LeftSuper) || held(Key::RightSuper)))
        return false;
    return true;
}

auto InputSystem::IsChordPressed(const KeyChord& chord) const -> bool {
    return IsKeyPressed(chord.key) && ModifierHeld(chord.modifiers);
}

auto InputSystem::IsChordHeld(const KeyChord& chord) const -> bool {
    return IsKeyHeld(chord.key) && ModifierHeld(chord.modifiers);
}

auto InputSystem::IsChordReleased(const KeyChord& chord) const -> bool {
    return IsKeyReleased(chord.key) && ModifierHeld(chord.modifiers);
}

} // namespace atom::input

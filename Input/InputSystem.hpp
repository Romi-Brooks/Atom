/**
 * @file           : Input/InputSystem.hpp
 * @author         : Romi Brooks
 * @brief          : Per-frame input state: keyboard/mouse edges, mouse position,
 *                   and action queries built on top of the normalized window
 *                   event stream.
 *
 * The backend produces raw `window::IEvent`s; InputSystem folds them into a
 * per-frame state machine so game code can ask "is this key held this frame?"
 * instead of reconstructing state from a raw event stream.
 *
 * Frame model:
 *   - BeginFrame() flips the previous frame's edges into steady state
 *     (Pressed -> Held, Released -> Up) and zeroes mouse delta/wheel.
 *   - ProcessEvent() applies one engine event (press/release/motion/wheel).
 *   - Game logic then reads IsKeyPressed/Held/Released and the action helpers.
 *
 * Single-threaded by design: driven by the main/render thread alongside event
 * polling, so no internal locking is needed.
 *
 * Two ways to read input, and they are not interchangeable (CORE-002):
 *
 *   Screen::HandleEvent(IEvent)   - event style, one event at a time.
 *       Waiting for: a discrete occurrence ("this key went down").
 *       Good for:    one-shot commands that fire once per press and need the
 *                    exact event payload - a modifier snapshot, a mouse
 *                    position captured together with the click, or a screen
 *                    that consumes the event (returning true stops the
 *                    dispatch).
 *       Limit:       it cannot express "held", "moved by", or "accumulated
 *                    over this frame". There is no frame to ask, and the
 *                    modifier bits are frozen into the event.
 *
 *   InputSystem (this class)       - polling style, one frame of state.
 *       Waiting for: the state the engine settled at BeginFrame().
 *       Good for:    continuous input (movement, drag, camera) and stable edge
 *                    queries that several places may ask in the same frame.
 *       Limit:       an edge is only observable during the frame it happened in,
 *                    and the state is global: it reports the raw keyboard even
 *                    while a UI text field is being typed into. That is what the
 *                    UI-capture flags below are for - see "UI capture".
 *
 * Rule of thumb: a command that is decided by one event belongs in
 * HandleEvent(); anything that is read more than once per frame, or that
 * depends on how long a key is down, belongs here.
 * @attention      :
 * @date           : 2026/10/15
 * Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_INPUT_INPUT_SYSTEM_HPP
#define ATOM_INPUT_INPUT_SYSTEM_HPP

#include <Input/ActionMap.hpp>
#include <Input/Key.hpp>

#include <Algorithm/Vector/Vec2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace atom::window {
struct IEvent;
}

namespace atom::input {

// Per-key/per-button frame state.
//   Up      - not pressed (and was not pressed this frame)
//   Pressed - pressed this frame (edge)
//   Held    - pressed now and also previously (held)
//   Released- released this frame (edge)
enum class KeyState : std::uint8_t { Up, Pressed, Held, Released };

class InputSystem {
    public:
        [[nodiscard]] static auto GetInstance() -> InputSystem&;

        InputSystem(const InputSystem&) = delete;
        auto operator=(const InputSystem&) -> InputSystem& = delete;

        // Called once per frame, before polling events. Flips the previous
        // frame's edges to steady state and clears mouse delta / wheel.
        //
        // RenderWindow::Run() calls this automatically at the start of its event
        // poll, so application code must not call it again inside the same
        // frame: a second call would settle a real press into Held before
        // anyone could observe the Pressed edge. Call it directly only when
        // driving the event stream by hand (tests, tools).
        auto BeginFrame() -> void;

        // Clears keyboard and mouse-button state, so no stale "held" edge
        // survives an event gap. RenderWindow feeds focus-loss events here
        // automatically; screens that manage their own "paused" state may also
        // call it when switching screens.
        //
        // This deliberately does NOT clear the mouse position, because the
        // cursor keeps a real position while the window is unfocused. Resetting
        // it would make the first motion after refocus report a bogus delta
        // from (0, 0) instead of from the true previous position. Call
        // ResetMousePosition() as well if that is not what you want.
        auto Reset() -> void;

        // Drops the stored mouse position and the "have a position" baseline,
        // so the next motion event re-establishes it without producing delta.
        auto ResetMousePosition() -> void;

        // Folds one normalized window event into the current frame's state.
        auto ProcessEvent(const window::IEvent& event) -> void;

        // --- UI capture -----------------------------------------------------
        // Any UI layer - a game's own chat box, inventory, menu, or the
        // debugger's ImGui overlay - reports what it currently owns. Nothing
        // here depends on a UI framework: the game UI calls it directly, and
        // the debug overlay calls the same entry point (see OverlayManager), so
        // the semantics do not change between a Debug build with the debugger
        // and a Release build without it.
        //
        // The state machine above always reports the raw device state. These
        // flags are how an application decides whether that raw state belongs to
        // the game or to the UI; the engine never suppresses input on its own,
        // because "does a click on a panel count as a click on the game" is a
        // game decision.
        //
        // Two keyboard granularities, because they need different answers:
        //   IsKeyboardCapturedByUi() - a UI layer owns the keyboard input
        //       context. Gate one-shot commands with this: while a chat box is
        //       open, typing "r" must not also recenter the world.
        //   IsTextInputCapturedByUi() - a text field is being edited right now.
        //       Gate continuous input (held movement keys) with this: while the
        //       player is typing a message, WASD must not walk the character.
        //
        // Feed it once per frame, after the UI layer has decided its own state.
        //
        // This is the *game's* claim: it states what the game's own UI owns, so
        // it fully replaces the previous game claim. Layers that only own part
        // of the input must not call this - the debug overlay uses
        // SetOverlayMouseCapture() so that its mouse claim cannot erase the
        // game's keyboard claim.
        auto SetUiCapture(const bool mouse, const bool keyboard, const bool text_input,
                          const std::string_view source = {}) -> void {
            game_mouse_captured_ = mouse;
            game_keyboard_captured_ = keyboard;
            game_text_input_captured_ = text_input;
            game_capture_source_ = (mouse || keyboard || text_input) ? source : std::string_view{};
        }

        // Mouse-only claim used by the engine's own debug overlay. It composes
        // with the game claim instead of replacing it: the overlay knows nothing
        // about the game's keyboard, so it must not be able to clear it. Without
        // this separation the overlay's per-frame call erased the game's chat
        // claim as soon as the cursor touched a debug panel, and movement kept
        // driving the scene while the player was typing.
        auto SetOverlayMouseCapture(bool captured) -> void {
            overlay_mouse_captured_ = captured;
        }

        [[nodiscard]] auto IsMouseCapturedByUi() const -> bool {
            return game_mouse_captured_ || overlay_mouse_captured_;
        }
        [[nodiscard]] auto IsKeyboardCapturedByUi() const -> bool {
            return game_keyboard_captured_;
        }
        [[nodiscard]] auto IsTextInputCapturedByUi() const -> bool {
            return game_text_input_captured_;
        }
        // Name of the layer that last claimed capture, for diagnostics. Empty
        // when capture is off.
        [[nodiscard]] auto UiCaptureSource() const -> std::string_view {
            if (!game_capture_source_.empty())
                return game_capture_source_;
            return overlay_mouse_captured_ ? std::string_view{"overlay"} : std::string_view{};
        }

        // --- Keyboard -------------------------------------------------------
        [[nodiscard]] auto IsKeyPressed(Key key) const -> bool;  // pressed this frame
        [[nodiscard]] auto IsKeyHeld(Key key) const -> bool;     // pressed now (incl. this frame)
        [[nodiscard]] auto IsKeyReleased(Key key) const -> bool; // released this frame
        [[nodiscard]] auto GetKeyState(Key key) const -> KeyState;

        // --- Mouse ----------------------------------------------------------
        [[nodiscard]] auto GetMousePosition() const -> algo::Vec2;
        [[nodiscard]] auto GetMouseDelta() const -> algo::Vec2; // accumulated motion this frame
        [[nodiscard]] auto GetMouseWheel() const -> algo::Vec2; // accumulated wheel this frame
        [[nodiscard]] auto IsMousePressed(MouseButton button) const -> bool;
        [[nodiscard]] auto IsMouseHeld(MouseButton button) const -> bool;
        [[nodiscard]] auto IsMouseReleased(MouseButton button) const -> bool;

        // --- Chord / action -------------------------------------------------
        // A chord is a key plus required modifiers; the modifiers are checked
        // against the current held state, not a single event's snapshot. That is
        // what makes Ctrl+S fire whether Ctrl was pressed before or after S.
        // The cost is that IsChordReleased also requires the modifiers to still
        // be held: releasing Ctrl first and then S will not report the chord as
        // released. Use IsKeyReleased for "the key came up" semantics.
        [[nodiscard]] auto IsChordPressed(const KeyChord& chord) const -> bool;
        [[nodiscard]] auto IsChordHeld(const KeyChord& chord) const -> bool;
        [[nodiscard]] auto IsChordReleased(const KeyChord& chord) const -> bool;

        // True when any binding of `action` has the requested state this frame.
        template <typename Action>
        [[nodiscard]] auto IsActionPressed(const ActionMap<Action>& map, const Action action) const -> bool {
            for (const auto& binding : map.GetBindings())
                if (binding.action == action && IsChordPressed(binding.chord))
                    return true;
            return false;
        }

        template <typename Action>
        [[nodiscard]] auto IsActionHeld(const ActionMap<Action>& map, const Action action) const -> bool {
            for (const auto& binding : map.GetBindings())
                if (binding.action == action && IsChordHeld(binding.chord))
                    return true;
            return false;
        }

        template <typename Action>
        [[nodiscard]] auto IsActionReleased(const ActionMap<Action>& map, const Action action) const -> bool {
            for (const auto& binding : map.GetBindings())
                if (binding.action == action && IsChordReleased(binding.chord))
                    return true;
            return false;
        }

    private:
        InputSystem() = default;

        [[nodiscard]] auto ModifierHeld(KeyModifiers modifiers) const -> bool;
        static auto SetDown(KeyState& state) -> void;
        static auto SetUp(KeyState& state) -> void;

        std::array<KeyState, KeyCount> keys_{};
        std::array<KeyState, MouseButtonCount> mouse_buttons_{};
        algo::Vec2 mouse_position_{};
        algo::Vec2 mouse_delta_{};
        algo::Vec2 mouse_wheel_{};
        std::string_view game_capture_source_{};
        bool has_mouse_position_ = false;
        bool game_mouse_captured_ = false;
        bool game_keyboard_captured_ = false;
        bool game_text_input_captured_ = false;
        bool overlay_mouse_captured_ = false;
};

} // namespace atom::input

#endif // ATOM_INPUT_INPUT_SYSTEM_HPP

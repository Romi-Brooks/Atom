/**
 * @file           : InputSystemTest.cpp
 * @brief          : Correctness tests for the per-frame input state machine.
 *
 * Covers the InputSystem's core contract independently of any backend:
 *   - key pressed / held / released edges across frames
 *   - key repeat does not re-raise the pressed edge
 *   - mouse position, delta (baseline-aware), and wheel accumulation
 *   - mouse button edges
 *   - chord queries with modifiers
 *   - action queries over an ActionMap
 *   - Reset() keeps the cursor position, ResetMousePosition() drops the baseline
 *   - Key::Unknown is dropped instead of aliasing slot 0
 *   - focus loss releases every held key and button
 *   - UI capture flags are reported without rewriting the raw device state
 */

#include <Input/InputSystem.hpp>

#include <Backend/Contracts/Window/IWindow.hpp>
#include <Test/Support/TestHelpers.hpp>

namespace {
using atom::input::ActionMap;
using atom::input::InputSystem;
using atom::input::Key;
using atom::input::KeyModifiers;
using atom::input::KeyState;
using atom::input::MouseButton;
using atom::window::EventType;
using atom::window::IEvent;
using atom::window::KeyEvent;
using atom::window::MouseEvent;
using atom::window::MouseWheelEvent;

auto MakeKey(const EventType type, const Key key, const KeyModifiers mods = KeyModifiers::None,
             const bool repeat = false) -> IEvent {
    IEvent event{};
    event.type = type;
    event.data = KeyEvent{key, mods, repeat};
    return event;
}

auto MakeMouseMove(const float x, const float y) -> IEvent {
    IEvent event{};
    event.type = EventType::MouseMoved;
    event.data = MouseEvent{x, y, MouseButton::Unknown};
    return event;
}

auto MakeMouseButton(const EventType type, const MouseButton button) -> IEvent {
    IEvent event{};
    event.type = type;
    event.data = MouseEvent{0.0f, 0.0f, button};
    return event;
}

auto MakeWheel(const float x, const float y) -> IEvent {
    IEvent event{};
    event.type = EventType::MouseWheel;
    event.data = MouseWheelEvent{x, y};
    return event;
}

enum class Action { Jump, Pause };

} // namespace

auto main() -> int {
    auto& input = InputSystem::GetInstance();
    input.Reset();

    // --- 1. Key edges across frames --------------------------------------
    {
        input.BeginFrame();
        ATOM_CHECK(!input.IsKeyPressed(Key::Space));
        ATOM_CHECK(!input.IsKeyHeld(Key::Space));

        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::Space));
        ATOM_CHECK(input.IsKeyPressed(Key::Space));
        ATOM_CHECK(input.IsKeyHeld(Key::Space));
        ATOM_CHECK(!input.IsKeyReleased(Key::Space));
        ATOM_CHECK(input.GetKeyState(Key::Space) == KeyState::Pressed);

        // Next frame: the pressed edge settles into held.
        input.BeginFrame();
        ATOM_CHECK(!input.IsKeyPressed(Key::Space));
        ATOM_CHECK(input.IsKeyHeld(Key::Space));

        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::Space));
        ATOM_CHECK(input.IsKeyReleased(Key::Space));
        ATOM_CHECK(!input.IsKeyHeld(Key::Space));

        // Next frame: released settles into up.
        input.BeginFrame();
        ATOM_CHECK(!input.IsKeyReleased(Key::Space));
        ATOM_CHECK(input.GetKeyState(Key::Space) == KeyState::Up);
    }

    // --- 2. Key repeat does not re-raise the pressed edge -----------------
    {
        input.Reset();
        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::A));
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::A, KeyModifiers::None, true));
        ATOM_CHECK(input.IsKeyPressed(Key::A));

        input.BeginFrame();
        ATOM_CHECK(!input.IsKeyPressed(Key::A));
        ATOM_CHECK(input.IsKeyHeld(Key::A));
        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::A));
        ATOM_CHECK(input.IsKeyReleased(Key::A));
    }

    // --- 3. Mouse position, delta, and wheel ------------------------------
    {
        input.Reset();
        input.BeginFrame();
        // First move establishes a baseline; no fabricated delta.
        input.ProcessEvent(MakeMouseMove(100.0f, 100.0f));
        ATOM_CHECK(input.GetMousePosition().GetX() == 100.0f);
        ATOM_CHECK(input.GetMouseDelta().GetX() == 0.0f);
        ATOM_CHECK(input.GetMouseDelta().GetY() == 0.0f);

        input.ProcessEvent(MakeMouseMove(110.0f, 105.0f));
        ATOM_CHECK(input.GetMouseDelta().GetX() == 10.0f);
        ATOM_CHECK(input.GetMouseDelta().GetY() == 5.0f);

        input.ProcessEvent(MakeWheel(0.0f, 2.0f));
        ATOM_CHECK(input.GetMouseWheel().GetY() == 2.0f);

        // Next frame clears delta and wheel.
        input.BeginFrame();
        ATOM_CHECK(input.GetMouseDelta().GetX() == 0.0f);
        ATOM_CHECK(input.GetMouseWheel().GetY() == 0.0f);
        // Position persists.
        ATOM_CHECK(input.GetMousePosition().GetX() == 110.0f);
    }

    // --- 4. Mouse button edges --------------------------------------------
    {
        input.Reset();
        input.BeginFrame();
        input.ProcessEvent(MakeMouseButton(EventType::MouseButtonPressed, MouseButton::Left));
        ATOM_CHECK(input.IsMousePressed(MouseButton::Left));
        ATOM_CHECK(input.IsMouseHeld(MouseButton::Left));

        input.BeginFrame();
        ATOM_CHECK(input.IsMouseHeld(MouseButton::Left));
        ATOM_CHECK(!input.IsMousePressed(MouseButton::Left));

        input.ProcessEvent(MakeMouseButton(EventType::MouseButtonReleased, MouseButton::Left));
        ATOM_CHECK(input.IsMouseReleased(MouseButton::Left));
        ATOM_CHECK(!input.IsMouseHeld(MouseButton::Left));
    }

    // --- 5. Chord queries with modifiers ----------------------------------
    {
        input.Reset();
        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::LeftControl));
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::S));

        // Ctrl+S requires the Control modifier to be held.
        ATOM_CHECK(input.IsChordPressed({Key::S, KeyModifiers::Control}));
        // Without Control held, the chord is not satisfied.
        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::LeftControl));
        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::S));
        input.BeginFrame();

        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::S));
        ATOM_CHECK(!input.IsChordPressed({Key::S, KeyModifiers::Control}));
        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::S));
    }

    // --- 6. Action queries over an ActionMap ------------------------------
    {
        ActionMap<Action> map{};
        ATOM_CHECK(map.Bind({Key::Space}, Action::Jump));
        ATOM_CHECK(map.Bind({Key::Up}, Action::Jump));
        ATOM_CHECK(map.Bind({Key::Escape}, Action::Pause));

        input.Reset();
        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::Space));
        ATOM_CHECK(input.IsActionPressed(map, Action::Jump));
        ATOM_CHECK(!input.IsActionPressed(map, Action::Pause));
        ATOM_CHECK(input.IsActionHeld(map, Action::Jump));

        // Arrow alternative for the same action.
        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::Space));
        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::Up));
        ATOM_CHECK(input.IsActionPressed(map, Action::Jump));
        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::Up));
    }

    // --- 7. Reset clears key/button state but keeps the cursor position -----
    {
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::W));
        input.ProcessEvent(MakeMouseMove(400.0f, 300.0f));
        input.Reset();
        ATOM_CHECK(!input.IsKeyHeld(Key::W));
        ATOM_CHECK(input.GetKeyState(Key::W) == KeyState::Up);
        // The cursor keeps a real position while the window is unfocused, so
        // Reset() must not invent a position of (0, 0): the next motion would
        // otherwise report a bogus delta from the origin.
        ATOM_CHECK(input.GetMousePosition().GetX() == 400.0f);
        ATOM_CHECK(input.GetMouseDelta().GetX() == 0.0f);

        // Dropping the baseline explicitly re-arms the "first move" path.
        input.ResetMousePosition();
        ATOM_CHECK(input.GetMousePosition().GetX() == 0.0f);
        input.ProcessEvent(MakeMouseMove(50.0f, 60.0f));
        ATOM_CHECK(input.GetMouseDelta().GetX() == 0.0f);
    }

    // --- 8. Unknown keys are dropped, not folded into slot 0 ---------------
    {
        input.Reset();
        input.BeginFrame();
        // An unmapped scancode arrives as Key::Unknown, which is also slot 0.
        // Folding it in would press whatever key sits at index 0.
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::Unknown));
        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::A));
        ATOM_CHECK(input.GetKeyState(Key::Unknown) == KeyState::Up);
        ATOM_CHECK(input.GetKeyState(Key::A) == KeyState::Up);
    }

    // --- 9. Losing focus releases everything ------------------------------
    {
        input.Reset();
        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::W));
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::LeftShift));
        input.ProcessEvent(MakeMouseButton(EventType::MouseButtonPressed, MouseButton::Left));
        ATOM_CHECK(input.IsKeyHeld(Key::W));
        ATOM_CHECK(input.IsMouseHeld(MouseButton::Left));

        // The OS stops sending key-up events once the window is unfocused, so
        // the engine folds the focus loss into a release instead.
        IEvent lost{};
        lost.type = EventType::FocusChanged;
        lost.data = atom::window::FocusEvent{false};
        input.ProcessEvent(lost);
        ATOM_CHECK(!input.IsKeyHeld(Key::W));
        ATOM_CHECK(!input.IsKeyHeld(Key::LeftShift));
        ATOM_CHECK(!input.IsMouseHeld(MouseButton::Left));

        // Regaining focus must be a no-op on its own.
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::W));
        IEvent gained{};
        gained.type = EventType::FocusChanged;
        gained.data = atom::window::FocusEvent{true};
        input.ProcessEvent(gained);
        ATOM_CHECK(input.IsKeyHeld(Key::W));
    }

    // --- 10. UI capture is reported but never rewrites the raw state --------
    {
        input.Reset();
        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::W));
        input.ProcessEvent(MakeMouseButton(EventType::MouseButtonPressed, MouseButton::Left));

        ATOM_CHECK(!input.IsMouseCapturedByUi());
        ATOM_CHECK(!input.IsKeyboardCapturedByUi());
        ATOM_CHECK(!input.IsTextInputCapturedByUi());
        ATOM_CHECK(input.UiCaptureSource().empty());

        // The overlay layer claiming the mouse does not change what the device
        // reports: application code decides what to do with the two facts.
        input.SetUiCapture(true, false, false, "ImGui overlay");
        ATOM_CHECK(input.IsMouseCapturedByUi());
        ATOM_CHECK(!input.IsKeyboardCapturedByUi());
        ATOM_CHECK(input.UiCaptureSource() == "ImGui overlay");
        ATOM_CHECK(input.IsMouseHeld(MouseButton::Left));
        ATOM_CHECK(input.IsKeyHeld(Key::W));

        // Coarse keyboard ownership (a focused panel) and the narrow text-input
        // gate are independent: only the field being typed into sets the latter.
        input.SetUiCapture(false, true, false, "ImGui overlay");
        ATOM_CHECK(input.IsKeyboardCapturedByUi());
        ATOM_CHECK(!input.IsTextInputCapturedByUi());

        input.SetUiCapture(false, true, true, "ImGui overlay");
        ATOM_CHECK(input.IsTextInputCapturedByUi());

        // Releasing the claim drops the stale source name with it.
        input.SetUiCapture(false, false, false, "ImGui overlay");
        ATOM_CHECK(!input.IsMouseCapturedByUi());
        ATOM_CHECK(!input.IsKeyboardCapturedByUi());
        ATOM_CHECK(!input.IsTextInputCapturedByUi());
        ATOM_CHECK(input.UiCaptureSource().empty());

        // Losing focus clears any capture claim already made.
        input.SetUiCapture(true, true, true, "ImGui overlay");
        IEvent lost{};
        lost.type = EventType::FocusChanged;
        lost.data = atom::window::FocusEvent{false};
        input.ProcessEvent(lost);
        ATOM_CHECK(!input.IsMouseCapturedByUi());
        ATOM_CHECK(!input.IsKeyboardCapturedByUi());
        ATOM_CHECK(!input.IsTextInputCapturedByUi());
    }

    // --- 11. The text gate, as a game UI actually drives it ---------------
    {
        // Reproduces the InputDemo chat box: the game opens a text UI from a
        // one-shot action, then must stop reading held movement keys while the
        // text field is being typed into. The frame order here mirrors the
        // engine (BeginFrame -> events -> FixedUpdate -> Update), so the
        // documented one-frame lag is pinned instead of drifting silently.
        enum class Action { OpenChat };
        ActionMap<Action> actions{};
        ATOM_CHECK(actions.Bind({Key::T}, Action::OpenChat));

        bool chat_open = false;
        input.Reset();
        input.SetUiCapture(false, false, false, {});

        // FixedUpdate(): returns true when movement would be applied.
        const auto fixed_update = [&] {
            return !input.IsTextInputCapturedByUi() && input.IsKeyHeld(Key::W);
        };
        // Update(): owns the chat state. A one-shot action that opens the UI must
        // be applied *before* the capture claim is published, otherwise the frame
        // that opens the chat still advertises "the game owns the keyboard".
        const auto variable_update = [&] {
            if (!chat_open && !input.IsKeyboardCapturedByUi() && input.IsActionPressed(actions, Action::OpenChat))
                chat_open = true;
            input.SetUiCapture(input.IsMouseCapturedByUi(), chat_open, chat_open, "chat box");
        };

        // Frame 1: W is held while the chat is still closed.
        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::W));
        ATOM_CHECK(fixed_update());
        variable_update();

        // Frame 2: T opens the chat. The claim is published in the same frame,
        // so the very next FixedUpdate is already gated.
        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::T));
        ATOM_CHECK(fixed_update());
        variable_update();
        ATOM_CHECK(chat_open);
        ATOM_CHECK(input.IsTextInputCapturedByUi());

        // Frame 3: W is still physically held, but movement must stop.
        input.BeginFrame();
        ATOM_CHECK(input.IsKeyHeld(Key::W));
        ATOM_CHECK(!fixed_update());
        variable_update();

        // Frame 4: and it stays stopped for as long as the chat is open.
        input.BeginFrame();
        ATOM_CHECK(!fixed_update());
        variable_update();

        // Closing the chat gives movement back immediately (no lag on release).
        chat_open = false;
        variable_update();
        ATOM_CHECK(!input.IsTextInputCapturedByUi());
        ATOM_CHECK(fixed_update());
    }

    // --- 12. Several fixed steps per frame: the claim must not lag ---------
    {
        // CORE-001 runs FixedUpdate 0..N times per frame, and it runs *before*
        // Update. So the frame in which a chat opens performs its fixed steps
        // before Update() publishes anything, and every step of that frame
        // moves: "typing does not stop the character until the next frame" is
        // not a one-frame nicety, it is a whole frame of movement per catch-up
        // step. The cure is to publish the claim at the moment the UI state
        // changes, not once at frame end.
        enum class Action { OpenChat };
        ActionMap<Action> actions{};
        ATOM_CHECK(actions.Bind({Key::T}, Action::OpenChat));

        bool chat_open = false;
        input.Reset();
        input.SetUiCapture(false, false, false, {});

        const auto publish = [&] { input.SetUiCapture(false, chat_open, chat_open, "chat box"); };
        const auto fixed_step = [&] { return !input.IsTextInputCapturedByUi() && input.IsKeyHeld(Key::W); };
        // Opening the UI happens on the *event* path, which runs before the
        // fixed steps of the same frame. That is the only place a claim can be
        // published early enough to gate them.
        const auto handle_event = [&](const atom::window::IEvent& event) {
            if (event.type != EventType::KeyPressed)
                return;
            const auto& key = std::get<KeyEvent>(event.data);
            if (chat_open || !actions.FindAction(key).has_value())
                return;
            chat_open = true;
            publish();
        };
        // Update() runs after the fixed steps, exactly as RenderWindow does it,
        // and only keeps the published claim current.
        const auto variable_update = [&] { publish(); };

        input.BeginFrame();
        input.ProcessEvent(MakeKey(EventType::KeyPressed, Key::W));

        // Frame 1, three fixed steps, then Update. No chat yet, so all move.
        ATOM_CHECK(fixed_step());
        ATOM_CHECK(fixed_step());
        ATOM_CHECK(fixed_step());
        variable_update();

        // Frame 2, in engine order: the T event is handled, then the frame's
        // fixed steps run, and only then Update. Handling the event publishes
        // the claim, so all three steps of the very frame that opens the chat
        // are already gated. Publishing only at frame end lets all three
        // through, which is exactly the reported bug.
        input.BeginFrame();
        const auto opened = MakeKey(EventType::KeyPressed, Key::T);
        input.ProcessEvent(opened);
        handle_event(opened);
        ATOM_CHECK(chat_open);
        ATOM_CHECK(input.IsTextInputCapturedByUi());
        ATOM_CHECK(input.IsKeyHeld(Key::W));
        ATOM_CHECK(!fixed_step());
        ATOM_CHECK(!fixed_step());
        ATOM_CHECK(!fixed_step());
        variable_update();

        // And it stays gated in following frames.
        input.BeginFrame();
        ATOM_CHECK(!fixed_step());
        ATOM_CHECK(!fixed_step());
        variable_update();

        // Closing republishes immediately, so the next step moves again.
        chat_open = false;
        publish();
        ATOM_CHECK(fixed_step());
    }

    return 0;
}

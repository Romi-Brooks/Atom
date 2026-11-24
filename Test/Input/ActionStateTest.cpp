/**
 * @file           : ActionStateTest.cpp
 * @brief          : T3.2 - gameplay asks for actions, and one gate suspends them.
 *
 * Three properties are what make the action layer worth having, and each is asserted
 * against the real InputSystem (driven by events, exactly as the host drives it):
 *
 *   - a system's code does not change when the bindings do (rebinding test);
 *   - opposing directions cancel, and a diagonal is not faster (Movement test);
 *   - closing the gameplay gate neutralises every gameplay query at once, while the
 *     action lookup on the event path keeps working - otherwise pause would be a
 *     one-way trip.
 */

#include <Input/ActionState.hpp>

#include <Backend/Contracts/Window/IWindow.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstdint>

namespace {

using namespace atom;
using atom::input::ActionState;
using atom::input::InputSystem;
using atom::input::Key;
using atom::input::KeyChord;
using atom::window::EventType;
using atom::window::IEvent;
using atom::window::KeyEvent;

enum class Action : std::uint8_t { Left, Right, Up, Down, Jump, Pause };

auto MakeKey(const EventType type, const Key key) -> IEvent {
    IEvent event{};
    event.type = type;
    event.data = KeyEvent{key};
    return event;
}

auto Press(InputSystem& input, const Key key) -> void {
    input.ProcessEvent(MakeKey(EventType::KeyPressed, key));
}
auto Release(InputSystem& input, const Key key) -> void {
    input.ProcessEvent(MakeKey(EventType::KeyReleased, key));
}

} // namespace

auto main() -> int {
    auto& input = InputSystem::GetInstance();
    input.Reset();

    ActionState<Action> actions{input};
    actions.Bind({Key::A}, Action::Left);
    actions.Bind({Key::D}, Action::Right);
    actions.Bind({Key::W}, Action::Up);
    actions.Bind({Key::S}, Action::Down);
    actions.Bind({Key::Space}, Action::Jump);
    actions.Bind({Key::Escape}, Action::Pause);

    // --- 1. Held state becomes a movement vector --------------------------
    {
        input.BeginFrame();
        Press(input, Key::A);

        ATOM_CHECK(actions.IsHeld(Action::Left));
        ATOM_CHECK(!actions.IsHeld(Action::Right));
        const auto left = actions.Movement(Action::Left, Action::Right, Action::Up, Action::Down);
        ATOM_CHECK_NEAR(left.GetX(), -1.0F);
        ATOM_CHECK_NEAR(left.GetY(), 0.0F);

        // Opposing directions cancel rather than picking a winner.
        Press(input, Key::D);
        const auto cancelled = actions.Movement(Action::Left, Action::Right, Action::Up, Action::Down);
        ATOM_CHECK_NEAR(cancelled.GetX(), 0.0F);
        ATOM_CHECK_NEAR(cancelled.GetY(), 0.0F);

        Release(input, Key::A);
        const auto now_right = actions.Movement(Action::Left, Action::Right, Action::Up, Action::Down);
        ATOM_CHECK_NEAR(now_right.GetX(), 1.0F);

        // A diagonal is normalized: |v| == 1, so diagonal travel is not 1.41x faster.
        Press(input, Key::W);
        const auto diagonal = actions.Movement(Action::Left, Action::Right, Action::Up, Action::Down);
        ATOM_CHECK_NEAR(diagonal.GetX(), 0.7071067F, 1.0e-4F);
        ATOM_CHECK_NEAR(diagonal.GetY(), -0.7071067F, 1.0e-4F);
        ATOM_CHECK_NEAR(diagonal.Length(), 1.0F, 1.0e-4F);
    }

    // --- 2. Edges: pressed is one frame, held is not, released is one frame --
    {
        input.BeginFrame();
        Press(input, Key::Space);
        ATOM_CHECK(actions.IsPressed(Action::Jump));
        ATOM_CHECK(actions.IsHeld(Action::Jump));

        // Next frame the key is still down: held yes, pressed no. This is the
        // difference a "toggle" action depends on.
        input.BeginFrame();
        ATOM_CHECK(!actions.IsPressed(Action::Jump));
        ATOM_CHECK(actions.IsHeld(Action::Jump));

        input.ProcessEvent(MakeKey(EventType::KeyReleased, Key::Space));
        ATOM_CHECK(actions.IsReleased(Action::Jump));
        input.BeginFrame();
        ATOM_CHECK(!actions.IsReleased(Action::Jump));
    }

    // --- 3. The gate suspends gameplay but not the event path -------------
    {
        input.BeginFrame();
        Press(input, Key::A);
        Press(input, Key::Space);

        ATOM_CHECK(actions.IsHeld(Action::Left));

        actions.SetGameplayEnabled(false);
        ATOM_CHECK(!actions.IsGameplayEnabled());

        // Every gameplay query reads neutral, without unbinding anything.
        ATOM_CHECK(!actions.IsHeld(Action::Left));
        ATOM_CHECK(!actions.IsPressed(Action::Jump));
        const auto gated = actions.Movement(Action::Left, Action::Right, Action::Up, Action::Down);
        ATOM_CHECK_NEAR(gated.GetX(), 0.0F);
        ATOM_CHECK_NEAR(gated.GetY(), 0.0F);

        // Menu input must still work while paused: the pause key is found through the
        // bindings, which are not gated. (The screen handles it on the event path.)
        const auto pause = actions.FindAction(KeyEvent{Key::Escape});
        ATOM_CHECK(pause.has_value());
        ATOM_CHECK(*pause == Action::Pause);

        // Unpausing restores the same bindings, including the key that is still held.
        actions.SetGameplayEnabled(true);
        ATOM_CHECK(actions.IsHeld(Action::Left));

        Release(input, Key::A);
        Release(input, Key::Space);
    }

    // --- 4. Rebinding changes behaviour, not the caller's code ------------
    //
    // This is the acceptance criterion "the same gameplay works through different
    // bindings": the query below is exactly the one a movement system makes.
    {
        input.BeginFrame();
        Press(input, Key::A);
        ATOM_CHECK(actions.IsHeld(Action::Left));

        // Give movement to the arrow keys only. The system's call site is unchanged.
        input::ActionMap<Action> remapped{};
        remapped.Bind({Key::Left}, Action::Left);
        remapped.Bind({Key::Right}, Action::Right);
        actions.SetBindings(std::move(remapped));

        ATOM_CHECK(!actions.IsHeld(Action::Left)); // A is no longer bound
        Release(input, Key::A);

        input.BeginFrame();
        Press(input, Key::Left);
        ATOM_CHECK(actions.IsHeld(Action::Left));
        const auto moved = actions.Movement(Action::Left, Action::Right, Action::Up, Action::Down);
        ATOM_CHECK_NEAR(moved.GetX(), -1.0F);

        // And an unbound action is simply never true, rather than falling back to a key.
        ATOM_CHECK(!actions.IsPressed(Action::Jump));
        Release(input, Key::Left);
    }

    // --- 5. Several chords may drive one action ---------------------------
    {
        input::ActionMap<Action> both{};
        both.Bind({Key::A}, Action::Left);
        both.Bind({Key::Left}, Action::Left);
        actions.SetBindings(std::move(both));

        input.BeginFrame();
        Press(input, Key::A);
        ATOM_CHECK(actions.IsHeld(Action::Left));
        Release(input, Key::A);

        input.BeginFrame();
        Press(input, Key::Left);
        ATOM_CHECK(actions.IsHeld(Action::Left));
        Release(input, Key::Left);
    }

    return 0;
}

#include "GameInput.hpp"

#include <utility>

namespace atom::game {

auto GameInput::GetInstance() -> GameInput& {
    // Built with its defaults installed, not empty-then-configured-by-the-caller.
    //
    // This is not tidiness: the first version returned an empty map and expected boot to call
    // ApplyDefaultBindings(). Boot never did, so every action query was false and every key was
    // dead - while the engine tests, which install bindings themselves, stayed green. A
    // singleton whose usable state depends on a call someone must remember is a trap.
    static GameInput instance = [] {
        GameInput input{};
        input.ApplyDefaultBindings();
        return input;
    }();
    return instance;
}

auto GameInput::ApplyDefaultBindings() -> void {
    input::ActionMap<GameAction> map{};

    // Two chords per direction, because that is the first thing a player tries and it
    // costs nothing: an action may keep several bindings.
    map.Bind({input::Key::A}, GameAction::MoveLeft);
    map.Bind({input::Key::Left}, GameAction::MoveLeft);
    map.Bind({input::Key::D}, GameAction::MoveRight);
    map.Bind({input::Key::Right}, GameAction::MoveRight);
    map.Bind({input::Key::W}, GameAction::MoveUp);
    map.Bind({input::Key::Up}, GameAction::MoveUp);
    map.Bind({input::Key::S}, GameAction::MoveDown);
    map.Bind({input::Key::Down}, GameAction::MoveDown);

    map.Bind({input::Key::E}, GameAction::Interact);

    // Escape and Space both pause. Note this is a *menu* action: it is read on the event
    // path, so the gameplay gate does not swallow it - otherwise pausing would be a
    // one-way trip.
    map.Bind({input::Key::Space}, GameAction::Pause);
    map.Bind({input::Key::Escape}, GameAction::Pause);

    // Restart is also a menu action: reloading the level while paused has to work.
    map.Bind({input::Key::R}, GameAction::Restart);

    actions_.SetBindings(std::move(map));
}

} // namespace atom::game

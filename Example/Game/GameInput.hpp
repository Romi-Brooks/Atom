/**
 * @file GameInput.hpp
 * @brief The game's action vocabulary and its default bindings.
 *
 * This lives in the example, not in the engine, on purpose: the engine owns "how do I
 * ask whether an action is held", the game owns "what actions exist". A second game
 * would bring its own enum and its own bindings and reuse ActionState unchanged.
 *
 * Everything gameplay-side reads actions from here. No system compares a `Key`, which
 * is what makes a remap screen, arrow-key alternatives or a gamepad a change to this
 * file's bindings rather than a change to the systems.
 */

#ifndef ATOM_EXAMPLE_GAME_GAMEINPUT_HPP
#define ATOM_EXAMPLE_GAME_GAMEINPUT_HPP

#include <cstdint>

#include <Input/ActionState.hpp>
#include <Input/Key.hpp>

namespace atom::game {

enum class GameAction : std::uint8_t {
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    // One-shot gameplay action: exists so the demo has a non-movement action whose
    // "pressed once, gated by pause" behaviour is exercised.
    Interact,
    // Menu action. Handled on the event path (ActionMap::FindAction), so it keeps
    // working while the gameplay gate is closed - that is the pause/resume loop.
    Pause,
};

class GameInput final {
    public:
        [[nodiscard]] static auto GetInstance() -> GameInput&;

        [[nodiscard]] auto Actions() noexcept -> input::ActionState<GameAction>& {
            return actions_;
        }
        [[nodiscard]] auto Actions() const noexcept -> const input::ActionState<GameAction>& {
            return actions_;
        }

        // Installs the default profile: WASD and the arrow keys both drive movement,
        // E interacts, Escape and Space pause. Idempotent, so a "reset bindings" button
        // would call the same function.
        auto ApplyDefaultBindings() -> void;

        // Replaces the profile wholesale - what a key-remap screen does. Exposed so a
        // caller can prove gameplay works with a different set of keys.
        auto SetBindings(input::ActionMap<GameAction> map) -> void {
            actions_.SetBindings(std::move(map));
        }

    private:
        GameInput() = default;
        input::ActionState<GameAction> actions_{};
};

} // namespace atom::game

#endif // ATOM_EXAMPLE_GAME_GAMEINPUT_HPP

/**
 * @file ActionState.hpp
 * @brief Gameplay-facing input queries: actions in, axes and edges out.
 *
 * Two problems this exists to solve, both visible in the demo before it was added:
 *
 *  1. Gameplay systems were reading keys (`IsKeyHeld(Key::A)`). That hard-codes both
 *     the keyboard and the exact key into gameplay logic, so a second binding (arrow
 *     keys, a remap screen, a gamepad later) means editing the system.
 *  2. "Pause suspends gameplay input" was each system's own `if (paused) return;`.
 *     One system forgetting that line is a bug that only shows up when someone pauses.
 *
 * The vocabulary is deliberately NOT here: each application owns its action enum
 * (see ActionMap.hpp). This header only knows how to turn whatever enum into answers.
 *
 * Reserving a second binding source: every query funnels through IsHeld()/IsPressed()/
 * IsReleased(), which read one ActionMap. A second source (gamepad) therefore means
 * one new map and one extra lookup in those three functions - not a change at any call
 * site. The bindings themselves are swappable via SetBindings(), which is what a key
 * remap profile or a test uses.
 *
 * The gate blocks *gameplay* queries only. Menu and pause handling must keep working
 * while the world is paused, and they go through the event path
 * (`ActionMap::FindAction(event)`) which is never gated - that is what "pause blocks
 * gameplay input but not UI input" means in practice.
 */

#ifndef ATOM_INPUT_ACTIONSTATE_HPP
#define ATOM_INPUT_ACTIONSTATE_HPP

#include <cmath>
#include <concepts>
#include <cstdint>
#include <optional>
#include <utility>

#include <Algorithm/Vector/Vec2.hpp>
#include <Input/ActionMap.hpp>
#include <Input/InputSystem.hpp>

namespace atom::input {

template <std::equality_comparable Action> class ActionState final {
    public:
        // Defaults to the process-wide InputSystem; a test passes the same singleton
        // after Reset(), which is how the input tests already drive it.
        explicit ActionState(const InputSystem& input = InputSystem::GetInstance()) : input_(&input) {}

        // --- bindings -------------------------------------------------------

        // Rebinding a chord replaces its previous action. Several chords may map to one
        // action (WASD and the arrow keys), which is why the map is not 1:1.
        auto Bind(const KeyChord chord, const Action action) -> bool {
            return map_.Bind(chord, action);
        }
        [[nodiscard]] auto FindAction(const KeyEvent& event) const -> std::optional<Action> {
            return map_.FindAction(event);
        }
        // Replaces the whole profile. This is the seam a key-remap screen or a second
        // input source plugs into without touching gameplay code.
        auto SetBindings(ActionMap<Action> map) -> void {
            map_ = std::move(map);
        }
        [[nodiscard]] auto Bindings() const -> const ActionMap<Action>& {
            return map_;
        }
        auto ClearBindings() -> void {
            map_.Clear();
        }

        // --- gameplay gate --------------------------------------------------

        // While disabled every gameplay query below reads neutral, without touching the
        // bindings: unpausing must not have to re-bind anything.
        auto SetGameplayEnabled(const bool enabled) noexcept -> void {
            gameplay_enabled_ = enabled;
        }
        [[nodiscard]] auto IsGameplayEnabled() const noexcept -> bool {
            return gameplay_enabled_;
        }

        // --- queries --------------------------------------------------------

        [[nodiscard]] auto IsPressed(const Action action) const -> bool {
            return gameplay_enabled_ && input_->IsActionPressed(map_, action);
        }
        [[nodiscard]] auto IsHeld(const Action action) const -> bool {
            return gameplay_enabled_ && input_->IsActionHeld(map_, action);
        }
        [[nodiscard]] auto IsReleased(const Action action) const -> bool {
            return gameplay_enabled_ && input_->IsActionReleased(map_, action);
        }

        // -1, 0 or +1 for a pair of actions. An axis rather than a bool because movement
        // is naturally a range, and because a future analogue source can feed the same
        // value without changing a call site.
        [[nodiscard]] auto Axis(const Action negative, const Action positive) const -> float {
            const float towards_positive = IsHeld(positive) ? 1.0F : 0.0F;
            const float towards_negative = IsHeld(negative) ? 1.0F : 0.0F;
            return towards_positive - towards_negative;
        }

        // Screen-space movement vector, y pointing down (matching Renderer2D), already
        // normalized so a diagonal is not 1.41x faster. Returns {0,0} when the gate is
        // closed, when nothing is held, or when opposite directions cancel out.
        [[nodiscard]] auto Movement(const Action left, const Action right, const Action up, const Action down) const
            -> algo::Vec2 {
            const float x = Axis(left, right);
            const float y = Axis(up, down);
            const float squared = x * x + y * y;
            if (squared <= 0.0F)
                return algo::Vec2::Zero();
            // Axis-aligned movement is already unit length; only the diagonal needs the
            // divide, and doing it only there keeps the axis case exact.
            if (squared <= 1.0F)
                return {x, y};
            const float inverse = 1.0F / std::sqrt(squared);
            return {x * inverse, y * inverse};
        }

        // True when text entry owns the keyboard. Passed through so a system can check
        // it once instead of reaching for InputSystem - the point of this class is that
        // gameplay code has no other reason to touch InputSystem.
        [[nodiscard]] auto IsTextInputCapturedByUi() const -> bool {
            return input_->IsTextInputCapturedByUi();
        }

    private:
        const InputSystem* input_ = nullptr;
        ActionMap<Action> map_{};
        bool gameplay_enabled_ = true;
};

} // namespace atom::input

#endif // ATOM_INPUT_ACTIONSTATE_HPP

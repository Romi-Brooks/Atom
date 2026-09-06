/**
 * @file           : ActionMap.hpp
 * @author         : Romi Brooks
 * @brief          : Maps normalized input chords to application-defined actions.
 * @attention      : The engine owns Key; each application owns its action enum.
 * @date           : 2026/9/7
 * Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_EVENT_ACTION_MAP_HPP
#define ATOM_EVENT_ACTION_MAP_HPP

#include <algorithm>
#include <concepts>
#include <optional>
#include <vector>

#include <Event/Input.hpp>

namespace atom::event {

struct KeyChord {
        Key key = Key::Unknown;
        KeyModifiers modifiers = KeyModifiers::None;

        [[nodiscard]] constexpr auto Matches(const KeyEvent& event) const -> bool {
            return key == event.key && modifiers == event.modifiers;
        }
};

template <std::equality_comparable Action> class ActionMap {
    public:
        struct Binding {
                KeyChord chord;
                Action action;
        };

        // Rebinding a chord replaces its previous action. An action can retain
        // multiple chords, enabling alternatives such as arrow keys and WASD.
        auto Bind(const KeyChord chord, const Action action) -> bool {
            if (chord.key == Key::Unknown)
                return false;
            Unbind(chord);
            bindings_.push_back({chord, action});
            return true;
        }

        auto Unbind(const KeyChord chord) -> void {
            std::erase_if(bindings_, [&chord](const Binding& binding) {
                return binding.chord.key == chord.key && binding.chord.modifiers == chord.modifiers;
            });
        }

        auto Clear() -> void {
            bindings_.clear();
        }

        [[nodiscard]] auto FindAction(const KeyEvent& event) const -> std::optional<Action> {
            const auto found = std::ranges::find_if(
                bindings_, [&event](const Binding& binding) { return binding.chord.Matches(event); });
            if (found == bindings_.end())
                return std::nullopt;
            return found->action;
        }

        [[nodiscard]] auto GetBindings() const -> const std::vector<Binding>& {
            return bindings_;
        }

    private:
        std::vector<Binding> bindings_;
};

} // namespace atom::event

#endif // ATOM_EVENT_ACTION_MAP_HPP

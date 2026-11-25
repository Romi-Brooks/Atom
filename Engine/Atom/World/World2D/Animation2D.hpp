// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Animation2D.hpp
 * @brief Deterministic frame clips, named states and frame events for ECS sprites.
 */
#ifndef ATOM_WORLD2D_ANIMATION2D_HPP
#define ATOM_WORLD2D_ANIMATION2D_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Atom/Core/Algorithm/Geometry/Rect.hpp>
#include <Atom/World/ECS/EntityWorld.hpp>

namespace atom::world2d {

struct AnimationFrame2D final {
        algo::Rect source{};
        float duration_seconds = 0.1f;
        // Zero means no event. Nonzero IDs are game-defined and can be passed
        // straight to Runtime's GameEventQueue by the caller.
        std::uint32_t event_type = 0;
};

struct AnimationClip2D final {
        std::string name{};
        std::string texture_uri{};
        std::vector<AnimationFrame2D> frames{};
};

struct AnimationEvent2D final {
        ecs::Entity entity{};
        std::uint32_t type = 0;
};

class AnimationLibrary2D final {
    public:
        [[nodiscard]] auto AddClip(AnimationClip2D clip) -> bool;
        [[nodiscard]] auto BindState(std::string state, std::string clip_name) -> bool;
        [[nodiscard]] auto FindState(std::string_view state) const -> const AnimationClip2D*;
        // A game sends named triggers; only transition topology lives here.
        [[nodiscard]] auto AddTransition(std::string from, std::string trigger, std::string to) -> bool;
        [[nodiscard]] auto FindTransition(std::string_view from, std::string_view trigger) const -> std::string_view;

    private:
        std::unordered_map<std::string, AnimationClip2D> clips_{};
        std::unordered_map<std::string, std::string> states_{};
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> transitions_{};
};

class AnimationSystem2D final {
    public:
        explicit AnimationSystem2D(const AnimationLibrary2D& library) : library_(&library) {}

        // Changing state restarts the clip; reasserting the current state is a
        // no-op. Game rules choose transitions, this system owns frame timing.
        [[nodiscard]] auto SetState(ecs::EntityWorld& world, ecs::Entity entity, std::string_view state) const -> bool;
        [[nodiscard]] auto Trigger(ecs::EntityWorld& world, ecs::Entity entity,
                                   std::string_view trigger) const -> bool;
        // Call once per fixed step. Events crossed by this step are returned in
        // entity-index order, then frame order. The returned vector reuses storage.
        [[nodiscard]] auto Step(ecs::EntityWorld& world, float delta_seconds)
            -> const std::vector<AnimationEvent2D>&;

    private:
        const AnimationLibrary2D* library_ = nullptr;
        std::vector<AnimationEvent2D> events_{};
};

} // namespace atom::world2d

#endif // ATOM_WORLD2D_ANIMATION2D_HPP

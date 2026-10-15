/**
 * @file           : Components.hpp
 * @brief          : The engine's component vocabulary, and the component list that
 *                   drives EntityWorld's explicit instantiations.
 *
 * Two things live here on purpose:
 *
 *  1. The component types themselves. They are plain data: no virtual functions,
 *     no back-pointers to the world, no behaviour. Systems own behaviour.
 *  2. ATOM_ECS_COMPONENTS - the single list of component types in the engine.
 *     EntityWorld.cpp expands it to instantiate its backend templates, so adding
 *     a component is: define it here, add it to the list below, done.
 *
 * Keeping the list explicit (rather than auto-registering on first use) is what
 * lets the backend stay out of every public header: the instantiations happen in
 * one .cpp, and a translation unit that merely includes EntityWorld.hpp compiles
 * no backend templates at all.
**/

#ifndef ATOM_ECS_COMPONENTS_HPP
#define ATOM_ECS_COMPONENTS_HPP

#include <cstdint>
#include <string>

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::ecs {

// --- transform / motion -----------------------------------------------------

struct Position final {
        algo::Vec2 value{};
};

struct Velocity final {
        algo::Vec2 value{};
};

struct Rotation final {
        float radians = 0.0f;
};

struct Scale final {
        algo::Vec2 value{1.0f, 1.0f};
};

// --- identity ---------------------------------------------------------------

// Display name. Kept out of Entity on purpose: a name is optional data, and
// paying for a std::string on every entity would be wasteful.
struct Name final {
        std::string value{};
};

// --- gameplay ---------------------------------------------------------------

struct Health final {
        int32_t current = 0;
        int32_t maximum = 0;
};

// --- tags -------------------------------------------------------------------
//
// Empty types: presence is the whole datum. They carry no value, so the API
// refuses Patch() on them (see EntityWorld::Patch).

// Entity participates in simulation this frame.
struct Active final {};

// Entity is not moved by physics or animation.
struct Static final {};

// Entity should be destroyed at the end of the frame. A deferred-destroy tag
// rather than an immediate Destroy() call, so a system never structurally
// modifies the world it is iterating.
struct PendingDestroy final {};

} // namespace atom::ecs

// The engine's component list. Every type here must be default constructible and
// movable. Order does not matter; the list exists so EntityWorld.cpp can
// instantiate its backend templates once.
#define ATOM_ECS_COMPONENTS                                                                                            \
    ::atom::ecs::Position, ::atom::ecs::Velocity, ::atom::ecs::Rotation, ::atom::ecs::Scale, ::atom::ecs::Name,        \
        ::atom::ecs::Health, ::atom::ecs::Active, ::atom::ecs::Static, ::atom::ecs::PendingDestroy

#endif // ATOM_ECS_COMPONENTS_HPP

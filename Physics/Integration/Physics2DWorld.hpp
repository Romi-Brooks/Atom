// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Physics2DWorld.hpp
 * @brief The one place where an ECS world and a physics world are kept in step (T3.1b).
 *
 * Division of labour:
 *
 *   ECS           holds config + a body handle (PhysicsBody2D) and the simulation
 *                 transform (Transform2D). It never sees an IPhysicsBody.
 *   physics       owns bodies, shapes, contacts. It never sees an Entity.
 *   this class    is the only code that knows both, and therefore the only place that
 *                 can convert units or translate handles.
 *
 * What Step() does, in order, and why the order matters:
 *
 *   1. create bodies for entities that have a shell and no handle yet. Creating before
 *      stepping means an entity added this frame already collides this frame, instead of
 *      a frame late - which is exactly the kind of one-frame hole that makes gameplay
 *      feel unreliable.
 *   2. destroy bodies whose entity died. Done by scanning the tracked set, because ECS
 *      has no destruction callback: an entity that is destroyed would otherwise leave a
 *      body that the renderer still collides with but nobody can see.
 *   3. step the physics world with the fixed delta it is given.
 *   4. copy body transforms back into Transform2D, converting metres/y-up to
 *      pixels/y-down here and only here.
 *   5. translate the contact lifecycle into ContactEvent2D values for the caller to turn
 *      into whatever its event type is. This class does not know about GameEvent, so
 *      Physics does not depend on Runtime.
 *
 * `user_data` is how a callback finds its entity: the handle is encoded as
 * `(generation << 32) | index`, which round-trips through Entity::FromParts without
 * relying on backend internals. A decoded handle is always re-validated with IsAlive,
 * because an entity may have been destroyed between the contact and this frame.
 */

#ifndef ATOM_PHYSICS_INTEGRATION_PHYSICS2DWORLD_HPP
#define ATOM_PHYSICS_INTEGRATION_PHYSICS2DWORLD_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include <ECS/Components.hpp>
#include <ECS/EntityWorld.hpp>
#include <Error/Error.hpp>
#include <Physics/Contracts/IPhysicsBackend.hpp>
#include <Physics/Contracts/IPhysicsWorld.hpp>

namespace atom::physics::integration {

// Which side of a contact lifecycle this event belongs to. The values are ordered the way
// they are dispatched (begin, persist, end) so a consumer can sort or compare them.
enum class ContactPhase : std::uint8_t { Begin, Persist, End };

struct ContactEvent2D final {
        ContactPhase phase = ContactPhase::Begin;
        // Both handles were alive when the event was collected; a consumer must still treat
        // them as possibly-stale next frame, like any Entity.
        ecs::Entity a{};
        ecs::Entity b{};
        // True when at least one side is a sensor, so a listener can tell "touched" from
        // "was blocked" without querying the components again.
        bool sensor = false;
};

class Physics2DWorld final {
    public:
        // `pixels_per_meter` is the single number that ties world pixels to physics metres.
        // The doc's suggested range is 32..100; 100 keeps a pixel-sized sprite a
        // centimetre-sized body, which is numerically comfortable for the solver.
        [[nodiscard]] static auto Create(IPhysicsBackend& backend, float pixels_per_meter = 100.0F)
            -> std::unique_ptr<Physics2DWorld>;

        ~Physics2DWorld();

        Physics2DWorld(const Physics2DWorld&) = delete;
        auto operator=(const Physics2DWorld&) = delete;
        Physics2DWorld(Physics2DWorld&&) = delete;
        auto operator=(Physics2DWorld&&) -> Physics2DWorld& = delete;

        // One fixed step. Returns the contacts observed during it, in dispatch order.
        [[nodiscard]] auto Step(ecs::EntityWorld& entities, float fixed_delta_seconds)
            -> Result<std::vector<ContactEvent2D>>;

        [[nodiscard]] auto PixelsPerMeter() const noexcept -> float {
            return pixels_per_meter_;
        }
        // Live bodies. Diagnostics and tests use it to prove nothing leaked.
        [[nodiscard]] auto BodyCount() const noexcept -> std::size_t {
            return bodies_.size();
        }
        [[nodiscard]] auto Gravity() const -> algo::Vec2;
        auto SetGravity(algo::Vec2 meters_per_second_squared) -> void;

        // pixel <-> metre helpers, exposed so the demo's debug draw and any tool use the
        // same arithmetic instead of re-deriving it.
        [[nodiscard]] auto ToMeters(algo::Vec2 pixels) const -> algo::Vec2;
        [[nodiscard]] auto ToPixels(algo::Vec2 meters) const -> algo::Vec2;

    private:
        struct Impl;
        explicit Physics2DWorld(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> impl_;
        float pixels_per_meter_ = 100.0F;
        // entity -> handle, and handle -> entity, both kept by the integration layer.
        std::vector<ecs::Entity> bodies_{};
};

} // namespace atom::physics::integration

#endif // ATOM_PHYSICS_INTEGRATION_PHYSICS2DWORLD_HPP

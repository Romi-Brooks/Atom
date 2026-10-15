/**
 * @file           : EntityWorld.hpp
 * @brief          : Engine-owned entity/component store.
 *
 * Boundary rule for this module: **no third-party type appears in this header,
 * or in any other public header of the engine.** The component backend is an
 * implementation detail of EntityWorld.cpp; consumers see Entity, Result and
 * atom::Error, nothing else. That keeps the backend replaceable and keeps the
 * ECS out of the Lua binding surface.
 *
 * The API is typed on purpose: a system declares the components it operates on,
 * so there is no runtime type registry to keep in sync and no way for a
 * misspelled type name to fail silently at run time.
 *
 * Adding a component type is a two-line change: declare it in
 * ECS/Components.hpp and add it to the component list there. That list drives the
 * explicit instantiations in EntityWorld.cpp, which is what keeps the template
 * bodies out of this header and makes including it cost nothing.
 *
 * Why failure is checked here rather than left to the backend: the backend
 * reports misuse by asserting, and its assertions are compiled out in release
 * builds. The measured consequences of letting misuse through in a release build
 * are silent state corruption (a component value overwritten, a component
 * created on a destroyed entity) and wild-pointer crashes. Every fallible entry
 * point below performs the cheap non-asserting check itself and returns
 * atom::Error instead.
 *
 * Thread model: single-threaded, like the rest of the engine's update path.
 *
 * Not yet provided: component iteration. A system currently reaches one entity at
 * a time through the typed accessors. An iteration layer is a separate design - it
 * has to settle the order guarantee, how tag components are requested, and how a
 * framework drives a runtime component list - so it is deliberately absent rather
 * than half-built here.
**/

#ifndef ATOM_ECS_ENTITY_WORLD_HPP
#define ATOM_ECS_ENTITY_WORLD_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include <ECS/Entity.hpp>
#include <Error/Error.hpp>

namespace atom::ecs {

// Components live in relocatable pools, so they must be movable and destructible.
template <typename T>
concept Component = std::is_move_constructible_v<T> && std::is_move_assignable_v<T> && std::is_destructible_v<T> &&
                    !std::is_reference_v<T>;

// A component that carries a value. An empty type is a tag instead: presence is the
// whole datum, so there is nothing to read, write or replace. Splitting the two
// concepts is what makes `TryGet<Tag>()` a compile error rather than a pointer to
// nothing, and it is checked here rather than in each body.
template <typename T>
concept ValueComponent = Component<T> && !std::is_empty_v<T>;

class EntityWorld final {
    public:
        // Creates an empty world.
        [[nodiscard]] static auto Create() -> Result<std::unique_ptr<EntityWorld>>;

        ~EntityWorld();

        EntityWorld(const EntityWorld&) = delete;
        auto operator=(const EntityWorld&) = delete;
        EntityWorld(EntityWorld&&) = delete;
        auto operator=(EntityWorld&&) -> EntityWorld& = delete;

        // --- entities -------------------------------------------------------

        // Slots are recycled: a destroyed entity's index is handed out again with
        // a new generation, so old handles to it stay invalid.
        [[nodiscard]] auto CreateEntity() -> Entity;

        // True only for a handle this world issued and has not destroyed.
        [[nodiscard]] auto IsAlive(Entity entity) const noexcept -> bool;

        // Destroying an already-dead handle is an error, so a caller can tell
        // "I destroyed it" from "something else did".
        [[nodiscard]] auto Destroy(Entity entity) -> Result<void>;

        // Entities created and not yet destroyed.
        [[nodiscard]] auto AliveCount() const noexcept -> std::size_t;

        // Slots not currently backing a live entity, including the reserved index 0.
        // For diagnostics and tests, not for reasoning about the entity space.
        [[nodiscard]] auto FreeSlotCount() const noexcept -> std::size_t;

        // --- components -----------------------------------------------------

        // Presence and lifecycle apply to value components and tags alike.
        template <Component T> [[nodiscard]] auto Has(Entity entity) const -> bool;

        // Null when the entity is dead or does not own the component. Never
        // crashes, unlike the backend's own accessor.
        //
        // Value components only: a tag has no value to point at.
        template <ValueComponent T> [[nodiscard]] auto TryGet(Entity entity) noexcept -> T*;

        // Typed access that distinguishes the reasons, for callers that want to
        // tell "the entity is gone" from "the component is absent".
        template <ValueComponent T> [[nodiscard]] auto Get(Entity entity) -> Result<T*>;

        // Adds a component. Fails when the entity is dead or already owns one, so
        // a silent overwrite is impossible by construction. Works for tags too:
        // `Emplace<Active>(entity)` is how an entity is marked.
        template <Component T, typename... Args> auto Emplace(Entity entity, Args&&... args) -> Result<void>;

        // Replace means "overwrite the stored value", which only exists for a value
        // component. For a tag, removing and re-adding is a no-op; use Remove.
        template <ValueComponent T, typename... Args> auto EmplaceOrReplace(Entity entity, Args&&... args)
            -> Result<void>;

        // Removal is idempotent: "make it absent" is satisfied either way.
        template <Component T> auto Remove(Entity entity) -> Result<void>;

        // Overwrites the value of a component that must already be present.
        template <ValueComponent T> auto Patch(Entity entity, const T& value) -> Result<void>;

        // Number of entities owning T. Works for tags: it counts entities marked
        // with the tag.
        template <Component T> [[nodiscard]] auto CountOf() const -> std::size_t;

        // Empty types are tags: presence is the whole datum.
        template <Component T> [[nodiscard]] static constexpr auto IsTag() noexcept -> bool {
            return std::is_empty_v<T>;
        }

        // --- iteration ------------------------------------------------------

        // What a component type is presented as to a visitor: a span over the single
        // instance that belongs to the entity being visited. A span (length 1) rather
        // than a reference keeps the parameter list uniform and keeps the loop body
        // writable. Only value components can be listed; see ForEach below.
        template <typename T> using VisitValue = std::span<T>;

        // The typed visit callback. Parameters are the entity, then one
        // VisitValue<T> per listed component type, in the order listed:
        //
        //   struct Integrate {
        //       static auto Run(void* context, Entity entity,
        //                       std::span<Position> positions, std::span<Velocity> velocities) -> void {
        //           positions[0].value = positions[0].value + velocities[0].value * *static_cast<float*>(context);
        //       }
        //   };
        //   world.ForEach<Position, Velocity>({&Integrate::Run, &delta_time});
        //
        // The shape is a function pointer plus a context, not a template lambda, and
        // that is deliberate: this header must not name the backend, so the loop
        // body cannot be a template here. It has to be compiled in a translation
        // unit that includes the backend headers, and a function pointer is what a
        // backend-free header can carry. The typed parameters still arrive with
        // their real types, so the body compiles against the true types and there
        // is no void* casting at the call site.
        //
        // A tag component (empty type) is presented as a length-0 span. Presence is
        // the datum, so there is nothing to read or write.
        //
        // Visit order is ascending entity index, matching With(). Structurally
        // changing the world from inside the body is refused by the mutating entry
        // points; mark entities with PendingDestroy and sweep after the pass.
        template <Component... Ts> using Visitor = void (*)(void* context, Entity entity, VisitValue<Ts>... components);

        template <Component... Ts> struct Visit final {
                Visitor<Ts...> visit = nullptr;
                void* context = nullptr;
        };

        // Visits every entity owning all of the listed components.
        template <ValueComponent... Ts> auto ForEach(Visit<Ts...> visit) -> Result<void>;

        // --- per-entity collection ------------------------------------------

        // Appends every entity that owns all of the listed components, in
        // ascending entity index order:
        //
        //   for (const auto entity : world.With<Position, Velocity>()) { ... }
        //
        // Order is ascending index, not the backend's pool order: physics and
        // animation want reproducible stepping, and a stable order is also what
        // lets a test assert something about a sequence of updates.
        //
        // Tag components (Active, Static, PendingDestroy) are not listable because
        // they have no value; check them per entity with Has<T>(). ForEach has the
        // same rule, and takes `bool` in place of a tag.
        //
        // Prefer ForEach for a hot loop: this entry point materialises a vector,
        // while ForEach walks the pools in place.
        template <Component... Ts> [[nodiscard]] auto With() const -> std::vector<Entity>;

        // --- backend access -------------------------------------------------

        // Raw backend handle for one entity, for code that must cross into a
        // backend-aware translation unit (physics integration, snapshot writers).
        // Not a public identity: it carries none of the generation checking that
        // makes Entity safe, so callers must not store it.
        [[nodiscard]] auto BackendHandle(Entity entity) const -> std::uint64_t;

    private:
        struct Impl;
        explicit EntityWorld(std::unique_ptr<Impl> impl);

        // Validates a handle against the engine's own slot table.
        [[nodiscard]] auto CheckAlive(Entity entity) const -> Result<void>;

        // Backend translations. No validation: the public templates above are the
        // only callers and they check first. Defined in the .cpp and explicitly
        // instantiated for the components listed in ECS/Components.hpp.
        template <Component T> [[nodiscard]] auto BackendHas(Entity entity) const -> bool;
        template <ValueComponent T> [[nodiscard]] auto BackendGet(Entity entity) noexcept -> T*;
        template <Component T, typename... Args> auto BackendEmplace(Entity entity, Args&&... args) -> void;
        template <Component T> auto BackendRemove(Entity entity) -> void;
        template <Component T> [[nodiscard]] auto BackendCount() const -> std::size_t;

        // Appends the entities owning all of Ts, sorted by engine index.
        template <Component... Ts> auto BackendWith(std::vector<Entity>& output) const -> void;

        // Runs the visitor once per matching entity, in the backend's pool order.
        template <ValueComponent... Ts> auto BackendForEach(Visit<Ts...> visit) -> Result<void>;

        std::unique_ptr<Impl> impl_;
};

// --- template bodies --------------------------------------------------------

template <Component T> auto EntityWorld::Has(const Entity entity) const -> bool {
    if (!IsAlive(entity))
        return false;
    return BackendHas<T>(entity);
}

template <ValueComponent T> auto EntityWorld::TryGet(const Entity entity) noexcept -> T* {
    if (!IsAlive(entity))
        return nullptr;
    return BackendGet<T>(entity);
}

template <ValueComponent T> auto EntityWorld::Get(const Entity entity) -> Result<T*> {
    if (auto alive = CheckAlive(entity); !alive.has_value())
        return std::unexpected<Error>{std::move(alive.error())};
    if (auto* existing = BackendGet<T>(entity); existing != nullptr)
        return existing;
    return std::unexpected<Error>{
        MakeError(ErrorCode::EcsComponentMissing, Module::Ecs, "EntityWorld::Get").WithDetail(typeid(T).name())};
}

template <Component T, typename... Args> auto EntityWorld::Emplace(const Entity entity, Args&&... args) -> Result<void> {
    if (auto alive = CheckAlive(entity); !alive.has_value())
        return alive;
    if (BackendHas<T>(entity)) {
        return std::unexpected<Error>{
            MakeError(ErrorCode::EcsComponentAlreadyPresent, Module::Ecs, "EntityWorld::Emplace")
                .WithDetail(typeid(T).name())};
    }
    BackendEmplace<T>(entity, std::forward<Args>(args)...);
    return Ok();
}

template <ValueComponent T, typename... Args>
auto EntityWorld::EmplaceOrReplace(const Entity entity, Args&&... args) -> Result<void> {
    if (auto alive = CheckAlive(entity); !alive.has_value())
        return alive;
    BackendRemove<T>(entity);
    BackendEmplace<T>(entity, std::forward<Args>(args)...);
    return Ok();
}

template <Component T> auto EntityWorld::Remove(const Entity entity) -> Result<void> {
    if (auto alive = CheckAlive(entity); !alive.has_value())
        return alive;
    BackendRemove<T>(entity);
    return Ok();
}

template <ValueComponent T> auto EntityWorld::Patch(const Entity entity, const T& value) -> Result<void> {
    auto existing = Get<T>(entity);
    if (!existing.has_value())
        return std::unexpected<Error>{std::move(existing.error())};
    **existing = value;
    return Ok();
}

template <Component T> auto EntityWorld::CountOf() const -> std::size_t {
    return BackendCount<T>();
}

template <Component... Ts> auto EntityWorld::With() const -> std::vector<Entity> {
    std::vector<Entity> entities{};
    BackendWith<Ts...>(entities);
    return entities;
}

template <ValueComponent... Ts> auto EntityWorld::ForEach(const Visit<Ts...> visit) -> Result<void> {
    if (visit.visit == nullptr) {
        return std::unexpected<Error>{
            MakeError(ErrorCode::EcsInvalidEntity, Module::Ecs, "EntityWorld::ForEach")
                .WithDetail("null visitor")};
    }
    return BackendForEach<Ts...>(visit);
}

} // namespace atom::ecs

#endif // ATOM_ECS_ENTITY_WORLD_HPP

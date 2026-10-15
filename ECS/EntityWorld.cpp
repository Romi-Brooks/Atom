/**
 * @file           : EntityWorld.cpp
 * @brief          : EntityWorld implementation. The only file in the engine that
 *                   includes the component backend.
 *
 * Layout, and why:
 *   1. Entity traits - the backend has to be told how to split the engine's
 *      handle into an index and a generation before it can key anything by Entity.
 *   2. Impl - the backend registry plus the engine's own slot table. The table is
 *      what makes a stale handle detectable.
 *   3. Backend translations - straight calls, no validation.
 *   4. Explicit instantiations, driven by ECS/Components.hpp.
 *
 * The backend's assertions are deliberately kept ALIVE here: they default to
 * <cassert>, which NDEBUG erases, and they are the last line of defence behind
 * this facade. See ECS/README.md.
**/

#include "EntityWorld.hpp"

// Keep the backend's assertions alive in every configuration. Must precede any
// backend header.
#if defined(NDEBUG)
#undef NDEBUG
#endif
#include <cassert>

#include <algorithm>
#include <string>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/entity/storage.hpp>

#include <ECS/Components.hpp>
#include <Log/LogSystem.hpp>

// --- 1. Entity traits -------------------------------------------------------
//
// The backend discovers a custom handle type through its nested `entity_type`
// alias and inherits the traits of that integral type. Specialising the inner
// traits is what gives the engine a full 32-bit generation instead of the
// backend's packed default, which spends only 12 bits on the version.

template <> struct entt::internal::entt_traits<atom::ecs::Entity> {
    using value_type = atom::ecs::Entity;
    using entity_type = std::uint32_t;
    using version_type = std::uint32_t;

    static constexpr entity_type entity_mask = 0xFFFFFFFF;
    static constexpr entity_type version_mask = 0xFFFFFFFF;
};

namespace atom::ecs {

// --- 2. Impl ----------------------------------------------------------------

struct EntityWorld::Impl final {
        // One entry per engine index. Index 0 is permanently dead: it is the
        // engine's invalid sentinel, and it is never allocated.
        struct Slot final {
                entt::entity handle = entt::null;
                bool alive = false;
        };

        entt::registry registry{};
        std::vector<Slot> slots{Slot{}};

        // The engine index is the backend index plus one. The offset is what keeps
        // the sentinel free: the backend's index space starts at 0, and after a
        // destroy its free list hands 0 back first, so index 0 cannot be reserved
        // by merely consuming it. Shifting the whole space by one sidesteps the
        // backend's numbering instead of fighting it.
        [[nodiscard]] static constexpr auto to_engine_index(const std::uint32_t backend_index) noexcept
            -> std::uint32_t {
            return backend_index + 1u;
        }

        [[nodiscard]] auto handle_of(const Entity entity) const noexcept -> entt::entity {
            return slots[entity.Index()].handle;
        }

        [[nodiscard]] auto is_alive(const Entity entity) const noexcept -> bool {
            if (!entity.IsValid() || entity.Index() >= slots.size())
                return false;
            const auto& slot = slots[entity.Index()];
            if (!slot.alive || slot.handle == entt::null)
                return false;
            return static_cast<std::uint32_t>(entt::to_version(slot.handle)) == entity.Generation();
        }
};

EntityWorld::EntityWorld(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

EntityWorld::~EntityWorld() = default;

auto EntityWorld::Create() -> Result<std::unique_ptr<EntityWorld>> {
    // Nothing to reserve: the engine's index space starts at 1 (see Impl).
    return std::unique_ptr<EntityWorld>{new EntityWorld{std::make_unique<Impl>()}};
}

// --- 3. Entities ------------------------------------------------------------

auto EntityWorld::CreateEntity() -> Entity {
    auto& impl = *impl_;

    // The backend owns entity numbering and versioning: a destroyed entity's index
    // returns to its free list, and reallocating that index yields the next
    // version, which is what invalidates the old handle. Reconstructing a handle
    // from an index would lose that version and produce a handle the backend
    // considers equal to the dead one.
    const auto handle = impl.registry.create();
    if (handle == entt::null) {
        // Ran out of entity slots. Reported rather than silently handing back an
        // invalid handle with no explanation.
        const auto error = MakeError(ErrorCode::EcsOutOfEntities, Module::Ecs, "EntityWorld::CreateEntity");
        LOG_ERROR(atom::log::core::Entity, error.Message());
        return Entity{};
    }

    const auto index = Impl::to_engine_index(static_cast<std::uint32_t>(entt::to_entity(handle)));
    if (index >= impl.slots.size())
        impl.slots.resize(static_cast<std::size_t>(index) + 1);
    impl.slots[index] = Impl::Slot{handle, true};
    return Entity::FromParts(index, static_cast<std::uint32_t>(entt::to_version(handle)));
}

auto EntityWorld::IsAlive(const Entity entity) const noexcept -> bool {
    return impl_->is_alive(entity);
}

auto EntityWorld::CheckAlive(const Entity entity) const -> Result<void> {
    if (!entity.IsValid()) {
        return std::unexpected<Error>{MakeError(ErrorCode::EcsInvalidEntity, Module::Ecs, "EntityWorld")
                                          .WithDetail("handle carries the reserved invalid index")};
    }
    if (!IsAlive(entity)) {
        // The usual way to get here: the entity was destroyed, its slot was
        // recycled, and this handle is from the previous generation.
        return std::unexpected<Error>{
            MakeError(ErrorCode::EcsEntityNotAlive, Module::Ecs, "EntityWorld")
                .WithDetail("index " + std::to_string(entity.Index()) + " generation " +
                            std::to_string(entity.Generation()))};
    }
    return Ok();
}

auto EntityWorld::Destroy(const Entity entity) -> Result<void> {
    if (auto alive = CheckAlive(entity); !alive.has_value())
        return alive;

    // The slot is cleared before the backend destroys the entity, so the handle
    // cannot be observed as alive through this table afterwards.
    const auto handle = impl_->handle_of(entity);
    impl_->slots[entity.Index()] = Impl::Slot{};
    impl_->registry.destroy(handle);
    return Ok();
}

auto EntityWorld::AliveCount() const noexcept -> std::size_t {
    // Counted from the slot table: the backend exposes no such query without
    // walking every storage, and the table is where aliveness is decided anyway.
    const auto& impl = *impl_;
    return static_cast<std::size_t>(std::count_if(impl.slots.begin(), impl.slots.end(), [](const Impl::Slot& slot) {
        return slot.alive && slot.handle != entt::null;
    }));
}

auto EntityWorld::FreeSlotCount() const noexcept -> std::size_t {
    const auto& impl = *impl_;
    return static_cast<std::size_t>(std::count_if(impl.slots.begin(), impl.slots.end(),
                                                  [](const Impl::Slot& slot) { return !slot.alive; }));
}

auto EntityWorld::BackendHandle(const Entity entity) const -> std::uint64_t {
    if (!IsAlive(entity))
        return 0;
    // The handle's bits are the engine's own layout, so this needs no backend call.
    return (static_cast<std::uint64_t>(entity.Generation()) << 32) | static_cast<std::uint64_t>(entity.Index());
}

// --- 4. Backend translations (no validation; the public templates check) ----

template <Component T> auto EntityWorld::BackendHas(const Entity entity) const -> bool {
    // `storage<T>()` creates the pool when it is missing, which is what makes
    // component types self-registering on first use.
    const auto& storage = const_cast<Impl&>(*impl_).registry.storage<T>();
    return storage.contains(impl_->handle_of(entity));
}

template <Component T> auto EntityWorld::BackendGet(const Entity entity) noexcept -> T* {
    auto& storage = impl_->registry.storage<T>();
    const auto handle = impl_->handle_of(entity);
    if (!storage.contains(handle))
        return nullptr;
    return &storage.get(handle);
}

template <Component T, typename... Args>
auto EntityWorld::BackendEmplace(const Entity entity, Args&&... args) -> void {
    auto& storage = impl_->registry.storage<T>();
    storage.emplace(impl_->handle_of(entity), std::forward<Args>(args)...);
}

template <Component T> auto EntityWorld::BackendRemove(const Entity entity) -> void {
    auto& storage = impl_->registry.storage<T>();
    const auto handle = impl_->handle_of(entity);
    if (storage.contains(handle))
        storage.remove(handle);
}

template <Component T> auto EntityWorld::BackendCount() const -> std::size_t {
    return const_cast<Impl&>(*impl_).registry.storage<T>().size();
}

template <Component... Ts> auto EntityWorld::BackendWith(std::vector<Entity>& output) const -> void {
    auto& impl = *impl_;

    // Walk the view, then sort by engine index. The backend's own order follows its
    // pool layout and changes as entities are destroyed; a stable order is worth
    // one sort here, because determinism is a property systems rely on.
    //
    // The visit only needs the entity, but the backend hands the components to the
    // callback as well. `[[maybe_unused]]` on the whole binding keeps the
    // intent explicit rather than looking like an oversight.
    impl.registry.view<Ts...>().each([&output]([[maybe_unused]] const entt::entity handle,
                                               [[maybe_unused]] Ts&... components) {
        const auto index = Impl::to_engine_index(static_cast<std::uint32_t>(entt::to_entity(handle)));
        output.push_back(Entity::FromParts(index, static_cast<std::uint32_t>(entt::to_version(handle))));
    });

    std::sort(output.begin(), output.end(), [](const Entity left, const Entity right) {
        return left.Index() < right.Index();
    });
}

// --- 5. Explicit instantiations ---------------------------------------------
//
// Driven by the engine's component list, so adding a component is a one-line
// change in ECS/Components.hpp and never touches this file. Explicit
// instantiation is also what keeps the backend template bodies out of the public
// header, which is what makes including it cheap.

// Value components carry data: they can be read, written and patched.
#define ATOM_ECS_INSTANTIATE(Component)                                                                                \
    template auto EntityWorld::Has<Component>(Entity) const -> bool;                                                   \
    template auto EntityWorld::TryGet<Component>(Entity) noexcept -> Component*;                                       \
    template auto EntityWorld::Get<Component>(Entity) -> Result<Component*>;                                           \
    template auto EntityWorld::Remove<Component>(Entity) -> Result<void>;                                              \
    template auto EntityWorld::Patch<Component>(Entity, const Component&) -> Result<void>;                             \
    template auto EntityWorld::CountOf<Component>() const -> std::size_t;                                              \
    template auto EntityWorld::BackendHas<Component>(Entity) const -> bool;                                            \
    template auto EntityWorld::BackendGet<Component>(Entity) noexcept -> Component*;                                   \
    template auto EntityWorld::BackendRemove<Component>(Entity) -> void;                                               \
    template auto EntityWorld::BackendCount<Component>() const -> std::size_t;                                         \
    template auto EntityWorld::BackendEmplace<Component, Component>(Entity, Component&&) -> void;

// Tag components are empty: presence is the whole datum. There is no value to
// read, write or patch and no address to hand out, so they get the smaller set.
#define ATOM_ECS_INSTANTIATE_TAG(Component)                                                                            \
    template auto EntityWorld::Has<Component>(Entity) const -> bool;                                                   \
    template auto EntityWorld::Remove<Component>(Entity) -> Result<void>;                                              \
    template auto EntityWorld::CountOf<Component>() const -> std::size_t;                                              \
    template auto EntityWorld::BackendHas<Component>(Entity) const -> bool;                                            \
    template auto EntityWorld::BackendRemove<Component>(Entity) -> void;                                               \
    template auto EntityWorld::BackendCount<Component>() const -> std::size_t;                                         \
    template auto EntityWorld::BackendEmplace<Component>(Entity) -> void;

ATOM_ECS_INSTANTIATE(::atom::ecs::Position)
ATOM_ECS_INSTANTIATE(::atom::ecs::Velocity)
ATOM_ECS_INSTANTIATE(::atom::ecs::Rotation)
ATOM_ECS_INSTANTIATE(::atom::ecs::Scale)
ATOM_ECS_INSTANTIATE(::atom::ecs::Name)
ATOM_ECS_INSTANTIATE(::atom::ecs::Health)

ATOM_ECS_INSTANTIATE_TAG(::atom::ecs::Active)
ATOM_ECS_INSTANTIATE_TAG(::atom::ecs::Static)
ATOM_ECS_INSTANTIATE_TAG(::atom::ecs::PendingDestroy)

// Iteration is instantiated per component set, not per component, so it has its
// own list. Add a line when a system starts iterating a new combination. Keeping
// the list short is the point: every set here costs compile time, and a set that
// no system uses is dead weight.
template auto EntityWorld::With<::atom::ecs::Position>() const -> std::vector<Entity>;
template auto EntityWorld::With<::atom::ecs::Position, ::atom::ecs::Velocity>() const -> std::vector<Entity>;
template auto EntityWorld::BackendWith<::atom::ecs::Position, ::atom::ecs::Velocity>(std::vector<Entity>&) const
    -> void;

} // namespace atom::ecs

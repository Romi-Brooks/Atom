/**
 * @file           : Entity.hpp
 * @brief          : Engine-owned entity handle: a 32-bit slot index plus a
 *                   32-bit generation.
 *
 * This type is deliberately NOT the underlying library's handle. Keeping our own
 * handle buys three things that matter for the rest of the engine:
 *
 *  1. Validity is decided here, by comparing generations, before any library
 *     call happens. The ECS backend reports most misuse by asserting, and its
 *     assertions are compiled out in release builds; a stale handle is therefore
 *     not something the backend can be trusted to catch.
 *  2. The handle survives a backend swap. Nothing outside this module knows what
 *     stores the components.
 *  3. The generation counter is ours, so it does not inherit the backend's
 *     version-bit ceiling. Slot reuse is bounded by 2^32 here, not by whatever
 *     bit width the backend packs into its id.
 *
 * A default-constructed Entity is never valid and is never handed out by
 * EntityWorld.
**/

#ifndef ATOM_ECS_ENTITY_HPP
#define ATOM_ECS_ENTITY_HPP

#include <cstdint>

namespace atom::ecs {

class Entity final {
    public:
        // 0 is reserved for "no entity". EntityWorld never returns it from
        // CreateEntity(), so it is safe as the invalid sentinel.
        static constexpr std::uint32_t invalid_index = 0;

        // Tells the storage backend how to split this handle into its parts. The
        // backend discovers custom entity types through this alias and inherits
        // the traits of the named integral type, so declaring it is what gives the
        // engine a full 32-bit generation instead of the backend's packed default
        // (which spends only 12 bits on the version, wrapping after 4096 recycles
        // of one slot).
        using entity_type = std::uint32_t;

        Entity() = default;

        // Rebuilds a handle from its parts. Public because the storage backend
        // has to construct handles back from the bits it stores; nothing else
        // should call it, and a caller who fabricates a handle only gets one
        // that fails EntityWorld's generation check.
        [[nodiscard]] static constexpr auto FromParts(const std::uint32_t index, const std::uint32_t generation) noexcept
            -> Entity {
            return Entity{index, generation};
        }

        [[nodiscard]] auto Index() const noexcept -> std::uint32_t {
            return index_;
        }
        [[nodiscard]] auto Generation() const noexcept -> std::uint32_t {
            return generation_;
        }

        // The only validity rule the handle itself can state: a handle with the
        // reserved index is invalid. Whether a *live* entity still backs this
        // handle is a question only EntityWorld can answer, because it owns the
        // slot table.
        [[nodiscard]] auto IsValid() const noexcept -> bool {
            return index_ != invalid_index;
        }

        [[nodiscard]] auto operator==(const Entity& other) const noexcept -> bool = default;

    private:
        constexpr Entity(const std::uint32_t index, const std::uint32_t generation) noexcept
            : index_(index), generation_(generation) {}

        std::uint32_t index_ = invalid_index;
        std::uint32_t generation_ = 0;
};

} // namespace atom::ecs

#endif // ATOM_ECS_ENTITY_HPP

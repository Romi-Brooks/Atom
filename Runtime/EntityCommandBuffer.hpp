/**
 * @file EntityCommandBuffer.hpp
 * @brief Deferred ECS structural changes at GameWorld phase boundaries.
 */
#ifndef ATOM_RUNTIME_ENTITYCOMMANDBUFFER_HPP
#define ATOM_RUNTIME_ENTITYCOMMANDBUFFER_HPP

#include <cstddef>
#include <functional>
#include <variant>
#include <vector>

#include <ECS/EntityWorld.hpp>

namespace atom::runtime {

class EntityCommandBuffer final {
    public:
        // Called after the entity exists. Use this to add the components that
        // define a spawned entity; do not retain the EntityWorld reference.
        using SpawnSetup = std::function<Result<void>(ecs::EntityWorld&, ecs::Entity)>;

        auto QueueCreate() -> void;
        auto QueueCreate(SpawnSetup setup) -> void;
        auto QueueDestroy(ecs::Entity entity) -> void;

        // Commands added while this method is applying a previous batch stay
        // queued for the next safe boundary.
        [[nodiscard]] auto Flush(ecs::EntityWorld& world) -> Result<void>;
        auto Clear() -> void;
        [[nodiscard]] auto Count() const noexcept -> std::size_t {
            return commands_.size();
        }

    private:
        struct CreateCommand final {
                SpawnSetup setup{};
        };
        struct DestroyCommand final {
                ecs::Entity entity{};
        };
        using Command = std::variant<CreateCommand, DestroyCommand>;

        std::vector<Command> commands_{};
};

} // namespace atom::runtime

#endif // ATOM_RUNTIME_ENTITYCOMMANDBUFFER_HPP

/**
 * @file GameWorld.hpp
 * @brief Runtime owner for one ECS game world and its deterministic systems.
 */
#ifndef ATOM_RUNTIME_GAMEWORLD_HPP
#define ATOM_RUNTIME_GAMEWORLD_HPP

#include <cstdint>
#include <memory>

#include <ECS/EntityWorld.hpp>
#include <Runtime/EntityCommandBuffer.hpp>
#include <Runtime/GameEventQueue.hpp>
#include <Runtime/SystemScheduler.hpp>

namespace atom::runtime {

class GameWorld final {
    public:
        [[nodiscard]] static auto Create() -> Result<std::unique_ptr<GameWorld>>;

        GameWorld(const GameWorld&) = delete;
        auto operator=(const GameWorld&) -> GameWorld& = delete;
        GameWorld(GameWorld&&) = delete;
        auto operator=(GameWorld&&) -> GameWorld& = delete;

        [[nodiscard]] auto GetEntities() noexcept -> ecs::EntityWorld& {
            return *entities_;
        }
        [[nodiscard]] auto GetEntities() const noexcept -> const ecs::EntityWorld& {
            return *entities_;
        }
        [[nodiscard]] auto GetScheduler() noexcept -> SystemScheduler& {
            return scheduler_;
        }
        [[nodiscard]] auto GetCommands() noexcept -> EntityCommandBuffer& {
            return commands_;
        }
        [[nodiscard]] auto GetEvents() noexcept -> GameEventQueue& {
            return events_;
        }

        // Runs phases in the only supported P0 order. Structural changes are
        // committed after each phase, never while a system is being visited.
        [[nodiscard]] auto RunFrame(float delta_seconds, std::uint32_t fixed_steps, float fixed_delta_seconds)
            -> Result<void>;
        [[nodiscard]] auto CommitStructuralChanges() -> Result<void>;
        [[nodiscard]] auto GetFrameIndex() const noexcept -> std::uint64_t {
            return frame_index_;
        }

        // Replaces the entity storage in one step - the scene switch (T2.2).
        //
        // Systems, events and the frame counter stay: a scene owns entities, not
        // behaviour, so loading a level must not silently unregister the systems a
        // screen configured. Pending structural commands and queued events are
        // dropped, because both may reference entities of the storage being
        // replaced.
        //
        // Returns the previous storage so the caller can decide when it dies; a
        // caller that does not care can ignore it and let it be destroyed here.
        auto ReplaceEntities(std::unique_ptr<ecs::EntityWorld> entities) -> std::unique_ptr<ecs::EntityWorld>;

        // Incremented by every ReplaceEntities. An Entity is (index, generation) and
        // the generation lives in the *slot table of one world*, so a handle cached
        // before a swap can be "alive" again afterwards by accident: a new world's
        // first entity is index 1 generation 0, exactly like the old world's first
        // entity. IsAlive() therefore cannot detect a stale handle across a swap.
        //
        // Systems that cache an Entity must cache this value too and re-resolve
        // (by SceneEntityId) when it changes. The alternative - looking the entity up
        // every frame - is always correct and is the right default until profiling
        // says otherwise.
        [[nodiscard]] auto GetWorldGeneration() const noexcept -> std::uint64_t {
            return world_generation_;
        }

    private:
        explicit GameWorld(std::unique_ptr<ecs::EntityWorld> entities) : entities_(std::move(entities)) {}
        [[nodiscard]] auto RunPhase(SystemPhase phase, float delta_seconds, std::uint32_t fixed_step_index)
            -> Result<void>;

        std::unique_ptr<ecs::EntityWorld> entities_;
        SystemScheduler scheduler_{};
        EntityCommandBuffer commands_{};
        GameEventQueue events_{};
        std::uint64_t frame_index_ = 0;
        std::uint64_t world_generation_ = 0;
};

} // namespace atom::runtime

#endif // ATOM_RUNTIME_GAMEWORLD_HPP

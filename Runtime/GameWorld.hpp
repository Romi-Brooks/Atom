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

    private:
        explicit GameWorld(std::unique_ptr<ecs::EntityWorld> entities) : entities_(std::move(entities)) {}
        [[nodiscard]] auto RunPhase(SystemPhase phase, float delta_seconds, std::uint32_t fixed_step_index)
            -> Result<void>;

        std::unique_ptr<ecs::EntityWorld> entities_;
        SystemScheduler scheduler_{};
        EntityCommandBuffer commands_{};
        GameEventQueue events_{};
        std::uint64_t frame_index_ = 0;
};

} // namespace atom::runtime

#endif // ATOM_RUNTIME_GAMEWORLD_HPP

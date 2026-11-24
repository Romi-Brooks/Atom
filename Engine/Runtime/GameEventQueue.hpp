/**
 * @file GameEventQueue.hpp
 * @brief Ordered, frame-local game events.
 */
#ifndef ATOM_RUNTIME_GAMEEVENTQUEUE_HPP
#define ATOM_RUNTIME_GAMEEVENTQUEUE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include <ECS/Entity.hpp>

namespace atom::runtime {

// Event types are game-defined numeric IDs. Runtime only guarantees FIFO
// delivery and does not reserve a global event registry before a game needs one.
using GameEventType = std::uint32_t;

struct GameEvent final {
        GameEventType type = 0;
        ecs::Entity source{};
        ecs::Entity target{};
};

class GameEventQueue final {
    public:
        auto Enqueue(GameEvent event) -> void;
        [[nodiscard]] auto Drain() -> std::vector<GameEvent>;
        auto Clear() -> void;
        [[nodiscard]] auto Count() const noexcept -> std::size_t {
            return events_.size();
        }

    private:
        std::vector<GameEvent> events_{};
};

} // namespace atom::runtime

#endif // ATOM_RUNTIME_GAMEEVENTQUEUE_HPP

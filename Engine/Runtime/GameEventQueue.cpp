#include "GameEventQueue.hpp"

#include <utility>

namespace atom::runtime {

auto GameEventQueue::Enqueue(GameEvent event) -> void {
    events_.push_back(event);
}

auto GameEventQueue::Drain() -> std::vector<GameEvent> {
    auto events = std::move(events_);
    events_.clear();
    return events;
}

auto GameEventQueue::Clear() -> void {
    events_.clear();
}

} // namespace atom::runtime

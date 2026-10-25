#include "GameWorld.hpp"

#include <cmath>
#include <utility>

namespace atom::runtime {

auto GameWorld::Create() -> Result<std::unique_ptr<GameWorld>> {
    auto entities = ecs::EntityWorld::Create();
    if (!entities)
        return std::unexpected<Error>{std::move(entities.error()).WithOperation("GameWorld::Create")};
    return std::unique_ptr<GameWorld>{new GameWorld{std::move(*entities)}};
}

auto GameWorld::RunFrame(const float delta_seconds, const std::uint32_t fixed_steps, const float fixed_delta_seconds)
    -> Result<void> {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f || !std::isfinite(fixed_delta_seconds) ||
        fixed_delta_seconds < 0.0f) {
        return std::unexpected<Error>{MakeError(ErrorCode::InvalidArgument, Module::Core, "GameWorld::RunFrame")
                                          .WithDetail("frame deltas must be finite and non-negative")};
    }

    if (auto result = RunPhase(SystemPhase::Input, delta_seconds, 0); !result)
        return result;
    for (std::uint32_t step = 0; step < fixed_steps; ++step) {
        if (auto result = RunPhase(SystemPhase::FixedUpdate, fixed_delta_seconds, step); !result)
            return result;
    }
    if (auto result = RunPhase(SystemPhase::Update, delta_seconds, 0); !result)
        return result;
    if (auto result = RunPhase(SystemPhase::LateUpdate, delta_seconds, 0); !result)
        return result;

    ++frame_index_;
    return Ok();
}

auto GameWorld::CommitStructuralChanges() -> Result<void> {
    return commands_.Flush(*entities_);
}

auto GameWorld::RunPhase(const SystemPhase phase, const float delta_seconds, const std::uint32_t fixed_step_index)
    -> Result<void> {
    const FrameContext context{phase, delta_seconds, frame_index_, fixed_step_index};
    if (auto result = scheduler_.Run(*this, context); !result)
        return result;
    return CommitStructuralChanges();
}

} // namespace atom::runtime

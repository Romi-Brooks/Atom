#include "GameScreen.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace atom::runtime {

GameScreen::GameScreen(WorldSetup setup, const float fixed_delta_seconds)
    : fixed_delta_seconds_(fixed_delta_seconds > 0.0F ? fixed_delta_seconds : 1.0F / 60.0F) {
    auto created = GameWorld::Create();
    if (!created.has_value())
        throw std::runtime_error("GameScreen: GameWorld::Create failed: " + created.error().Message());
    world_ = std::move(*created);

    if (!setup)
        return;
    if (auto configured = setup(*world_); !configured.has_value()) {
        // The world is torn down by unique_ptr; nothing else observed it yet.
        throw std::runtime_error("GameScreen: world setup failed: " + configured.error().Message());
    }
}

auto GameScreen::BeginFrame(const std::uint32_t fixed_steps, const float fixed_delta_seconds,
                            const float interpolation_alpha) -> void {
    // The host's budget for this frame. Stored rather than acted on: the world must
    // run between Update and Render, not here, so that a screen's setup and its
    // frame order stay in one place.
    pending_fixed_steps_ = fixed_steps;
    if (fixed_delta_seconds > 0.0F)
        fixed_delta_seconds_ = fixed_delta_seconds;
    // Clamped rather than trusted: a clock that is paused or not yet advanced can report
    // something outside [0,1), and interpolating with that would extrapolate.
    interpolation_alpha_ = interpolation_alpha < 0.0F ? 0.0F : (interpolation_alpha > 0.999F ? 0.999F : interpolation_alpha);
}

auto GameScreen::Update(const float deltaTime) -> void {
    const auto steps = pending_fixed_steps_;
    // Consume the budget: a frame that is dispatched twice (never happens in the
    // host, but tests do it) must not replay the previous frame's step count.
    pending_fixed_steps_ = 0;

    if (paused_)
        return;

    const auto delta = deltaTime > 0.0F ? deltaTime : 0.0F;
    if (auto advanced = world_->RunFrame(delta, steps, fixed_delta_seconds_); !advanced.has_value()) {
        // A world frame cannot be "half applied": RunFrame either commits its phases or
        // rejects the timing. Dropping the frame is the only safe choice, and a screen
        // that wants to react logs through its own channel.
        return;
    }
}

auto GameScreen::ReplaceWorldEntities(std::unique_ptr<ecs::EntityWorld> entities) -> void {
    // A null storage would leave the screen with no world to run; ignore it rather
    // than crash the loop, and keep the existing one.
    if (entities == nullptr)
        return;
    // The previous storage dies here: nothing in Runtime holds raw ECS pointers, and
    // a caller that needed the old world would have to keep it before swapping.
    (void)world_->ReplaceEntities(std::move(entities));
}

} // namespace atom::runtime

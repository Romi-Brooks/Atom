/**
 * @file GameScreen.hpp
 * @brief A Screen that owns one GameWorld and runs its fixed-step phases (T1.3).
 *
 * The frame-rhythm decision (G0.1) is made here and only here:
 *
 *   Host frame:   BeginFrame(N, dt) -> FixedUpdate x N (unused) -> Update -> Render
 *   This screen:  records N, then Update runs GameWorld::RunFrame(delta, N, dt)
 *
 * GameWorld::RunFrame already runs the only legal order - Input, FixedUpdate x N,
 * Update, LateUpdate, committing after each phase - so batching the host's N fixed
 * callbacks into that single call is semantically identical to per-step dispatch,
 * and is why this screen leaves FixedUpdate() unimplemented. The rejected
 * alternative was running a whole RunFrame inside FixedUpdate(), which would have
 * multiplied the Input/Update/LateUpdate phases by N and quietly broken the
 * ordering contract the GameWorld exists to provide.
 *
 * Rendering is NOT this class's job: subclasses submit their own pass in Render().
 * The world has already advanced for this frame by then, so anything they read
 * (including a Render/Scene2D snapshot) is committed state.
 *
 * This class is abstract - Render() and HandleEvent() stay the subclass's. Its
 * constructor is protected on purpose: a GameScreen must be constructed through a
 * concrete type, so there is no half-built screen that could reach the loop.
 */

#ifndef ATOM_RUNTIME_GAMESCREEN_HPP
#define ATOM_RUNTIME_GAMESCREEN_HPP

#include <cstdint>
#include <functional>
#include <memory>

#include <Error/Error.hpp>
#include <Runtime/GameWorld.hpp>
#include <Runtime/Screen.hpp>

namespace atom::runtime {

class GameScreen : public Screen {
    public:
        ~GameScreen() override = default;

        GameScreen(const GameScreen&) = delete;
        auto operator=(const GameScreen&) -> GameScreen& = delete;
        GameScreen(GameScreen&&) = delete;
        auto operator=(GameScreen&&) -> GameScreen& = delete;

        [[nodiscard]] auto World() noexcept -> GameWorld& {
            return *world_;
        }
        [[nodiscard]] auto World() const noexcept -> const GameWorld& {
            return *world_;
        }

        // --- Screen ---
        //
        // FixedUpdate is deliberately not overridden: the world's fixed steps run
        // inside Update's RunFrame batch. Overriding it would run the phases twice.
        auto BeginFrame(std::uint32_t fixed_steps, float fixed_delta_seconds, float interpolation_alpha) -> void override;
        auto Update(float deltaTime) -> void override;

        // Frames completed since construction (the world's frame index).
        [[nodiscard]] auto FrameCount() const noexcept -> std::uint64_t {
            return world_->GetFrameIndex();
        }

        // Hands the world a freshly loaded entity storage (T2.2). Runtime does not
        // depend on the Scene module on purpose: the game code calls SceneLoader and
        // passes the result here, so the level format never becomes part of the
        // frame-loop contract.
        //
        // Systems survive the swap; an Entity handle a system cached does not, which
        // is why scene entities are looked up by SceneEntityId.
        auto ReplaceWorldEntities(std::unique_ptr<ecs::EntityWorld> entities) -> void;

        // A paused screen still receives BeginFrame and Update from the host but
        // does not advance the world, so the frame index stops moving.
        // How far the current display frame sits between the last two fixed steps, in
        // [0,1). Pass it to Render/Scene2D so sprites are drawn between steps rather than
        // snapping to the newest one.
        [[nodiscard]] auto InterpolationAlpha() const noexcept -> float {
            return interpolation_alpha_;
        }

        [[nodiscard]] auto IsPaused() const noexcept -> bool {
            return paused_;
        }
        auto SetPaused(bool paused) noexcept -> void {
            paused_ = paused;
        }

    protected:
        // Called once, right after the world is created, to register systems and
        // seed entities. A failure throws std::runtime_error carrying the Error's
        // message: a screen with a half-configured world must never reach the loop,
        // and construction is the only place that can be refused before the host
        // takes ownership.
        using WorldSetup = std::function<Result<void>(GameWorld&)>;

        // `fixed_delta_seconds` is the fallback step used when the host reports no
        // game clock (a headless test or a tool driving the screen directly).
        explicit GameScreen(WorldSetup setup, float fixed_delta_seconds = 1.0F / 60.0F);


    private:
        std::unique_ptr<GameWorld> world_{};
        std::uint32_t pending_fixed_steps_ = 0;
        float fixed_delta_seconds_ = 1.0F / 60.0F;
        float interpolation_alpha_ = 0.0F;
        bool paused_ = false;
};

} // namespace atom::runtime

#endif // ATOM_RUNTIME_GAMESCREEN_HPP

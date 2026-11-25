#include "PromenadeScreen.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <ECS/Components.hpp>
#include <Input/InputSystem.hpp>

#include "PromenadeConfig.hpp"

namespace homeless {

namespace {

constexpr std::uint32_t foot_contact = 1;
constexpr float walk_speed = 74.0f;
constexpr float walk_acceleration = 780.0f;
constexpr float walk_braking = 1150.0f;

} // namespace

auto PromenadeScreen::SetUpWalkerAnimation() -> bool {
    atom::world2d::AnimationClip2D walk{.name = "walk", .texture_uri = std::string(walker_uri)};
    // Start with the feet close together so walking can begin from the idle pose.
    for (const int frame : {4, 5, 6, 7, 2, 3})
        walk.frames.push_back({.source = {32.0f * frame, 0, 32, 46},
                               .duration_seconds = 0.09f,
                               .event_type = frame == 3 || frame == 6 ? foot_contact : 0});
    if (!clips_.AddClip(std::move(walk)) ||
        !clips_.AddClip({.name = "idle",
                         .texture_uri = std::string(walker_uri),
                         .frames = {{{32, 0, 32, 46}, 0.8f}, {{0, 0, 32, 46}, 1.1f}}}) ||
        !clips_.BindState("walk", "walk") || !clips_.BindState("idle", "idle") ||
        !animation_.SetState(*world_, walker_, "idle"))
        return false;
    return true;
}

auto PromenadeScreen::PlayFootstep() -> void {
    if (mixer_.GetEffectiveSFXVolume() <= 0)
        return;
    const auto offset = 1 + walker_rng_.Below(SceneAudio::footstep_count - 1);
    last_footstep_ = (last_footstep_ + offset) % SceneAudio::footstep_count;
    (void)scene_audio_.Queue(static_cast<SceneAudio::Sound>(last_footstep_));
}

auto PromenadeScreen::UpdateBreath(float seconds, bool walking) -> void {
    next_breath_ -= seconds;
    if (next_breath_ > 0)
        return;
    const auto position = world_->TryGet<atom::ecs::Transform2D>(walker_)->position;
    const auto& sprite = *world_->TryGet<atom::ecs::SpriteRenderer>(walker_);
    const bool left = sprite.flip_x;
    const auto puffs = 2 + walker_rng_.Below(4);
    const float y = position.GetY() - 32.0f + walker_rng_.UniformFloat(-1.5f, 1.5f);
    (void)snow_.Burst(left ? "breath-left" : "breath-right", {position.GetX() + (left ? -9.0f : 9.0f), y}, puffs);
    next_breath_ = walking ? walker_rng_.UniformFloat(0.95f, 2.2f) : walker_rng_.UniformFloat(1.7f, 3.7f);
}

auto PromenadeScreen::UpdateWalker(float seconds) -> void {
    using atom::input::Key;
    const auto& input = atom::input::InputSystem::GetInstance();
    const bool left = input.IsKeyHeld(Key::A) || input.IsKeyHeld(Key::Left);
    const bool right = input.IsKeyHeld(Key::D) || input.IsKeyHeld(Key::Right);
    const int axis = static_cast<int>(right) - static_cast<int>(left);
    const float target_velocity = axis * walk_speed;
    const float rate = axis == 0 || walk_velocity_ * target_velocity < 0 ? walk_braking : walk_acceleration;
    walk_velocity_ += std::clamp(target_velocity - walk_velocity_, -rate * seconds, rate * seconds);
    auto& transform = *world_->TryGet<atom::ecs::Transform2D>(walker_);
    const float x = transform.position.GetX();
    const float proposed = x + walk_velocity_ * seconds;
    const float next = std::clamp(proposed, walk_boundary, world_width_ - walk_boundary);
    transform.position.SetX(next);
    if (next != proposed)
        walk_velocity_ = 0.0f;
    const bool walking = next != x;
    if (walking)
        world_->TryGet<atom::ecs::SpriteRenderer>(walker_)->flip_x = next < x;
    auto& animator = *world_->TryGet<atom::ecs::Animator2D>(walker_);
    if (CheckAreaExit(axis, next)) {
        animator.speed = 0.0f;
        return;
    }

    UpdateBreath(seconds, walking);
    (void)animation_.SetState(*world_, walker_, walking ? "walk" : "idle");
    // Drive the walk cycle by actual distance traveled, including acceleration and braking.
    animator.speed = walking ? std::abs(next - x) / (walk_speed * seconds) : 1.0f;
}

auto PromenadeScreen::StepAnimation(float seconds) -> void {
    for (const auto& event : animation_.Step(*world_, seconds)) {
        if (fade_ == Fade::None && event.entity == walker_ && event.type == foot_contact)
            PlayFootstep();
    }
}

} // namespace homeless

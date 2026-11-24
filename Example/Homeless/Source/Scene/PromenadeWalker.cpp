#include "PromenadeScreen.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include <ECS/Components.hpp>
#include <Input/InputSystem.hpp>
#include <Log/LogSystem.hpp>

#include "PromenadeConfig.hpp"

namespace homeless {

namespace {

constexpr std::uint32_t foot_contact = 1;
constexpr std::array footstep_ids{"snow-step-1", "snow-step-2", "snow-step-3"};

} // namespace

auto PromenadeScreen::SetUpWalkerAnimation() -> bool {
    atom::world2d::AnimationClip2D walk{.name = "walk", .texture_uri = std::string(walker_uri)};
    for (int i = 2; i < 8; ++i)
        walk.frames.push_back({.source = {32.0f * i, 0, 32, 46},
                               .duration_seconds = 0.105f,
                               .event_type = i == 3 || i == 6 ? foot_contact : 0});
    if (!clips_.AddClip(std::move(walk)) ||
        !clips_.AddClip({.name = "idle",
                         .texture_uri = std::string(walker_uri),
                         .frames = {{{0, 0, 32, 46}, 1.1f}, {{32, 0, 32, 46}, 0.8f}}}) ||
        !clips_.AddClip(
            {.name = "breath", .texture_uri = std::string(walker_uri), .frames = {{{32, 0, 32, 46}, 0.24f}}}) ||
        !clips_.BindState("breath", "breath") || !clips_.BindState("walk", "walk") ||
        !clips_.BindState("idle", "idle") || !animation_.SetState(*world_, walker_, "idle"))
        return false;
    (void)animation_.Step(*world_, 0.0f);
    return true;
}

auto PromenadeScreen::LoadFootsteps() -> void {
    for (std::size_t i = 0; i < footstep_ids.size(); ++i) {
        if (!effects_.Load(footstep_ids[i], "res://Audio/Footstep" + std::to_string(i + 1) + ".wav"))
            LOG_WARNING(atom::log::audio::Sfx, "Footstep " + std::to_string(i + 1) + " is unavailable");
    }
}

auto PromenadeScreen::PlayFootstep() -> void {
    const float volume = mixer_.GetEffectiveSFXVolume() * 0.23f;
    if (volume <= 0)
        return;
    const auto offset = 1 + walker_rng_.Below(footstep_ids.size() - 1);
    last_footstep_ = (last_footstep_ + offset) % footstep_ids.size();
    effects_.Play(footstep_ids[last_footstep_], volume);
}

auto PromenadeScreen::UpdateBreath(float seconds, bool walking) -> void {
    breath_pose_ = std::max(0.0f, breath_pose_ - seconds);
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
    breath_pose_ = walker_rng_.UniformFloat(0.14f, 0.28f);
}

auto PromenadeScreen::UpdateWalker(float seconds) -> void {
    using atom::input::Key;
    const auto& input = atom::input::InputSystem::GetInstance();
    const bool left = input.IsKeyHeld(Key::A) || input.IsKeyHeld(Key::Left);
    const bool right = input.IsKeyHeld(Key::D) || input.IsKeyHeld(Key::Right);
    const auto axis = static_cast<int>(right) - static_cast<int>(left);
    auto& transform = *world_->TryGet<atom::ecs::Transform2D>(walker_);
    const float x = transform.position.GetX();
    const float next = std::clamp(x + axis * 74.0f * seconds, 18.0f, world_width_ - 18.0f);
    transform.position.SetX(next);
    if (axis != 0)
        world_->TryGet<atom::ecs::SpriteRenderer>(walker_)->flip_x = axis < 0;
    UpdateBreath(seconds, next != x);
    const auto state = next != x ? "walk" : breath_pose_ > 0 ? "breath" : "idle";
    (void)animation_.SetState(*world_, walker_, state);
    for (const auto& event : animation_.Step(*world_, seconds)) {
        if (event.entity == walker_ && event.type == foot_contact)
            PlayFootstep();
    }
}

} // namespace homeless

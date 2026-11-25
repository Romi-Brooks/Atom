#include "Pedestrians.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <Atom/World/ECS/Components.hpp>

#include "PromenadeConfig.hpp"
#include "SceneSprite.hpp"

namespace homeless {

namespace {

constexpr float offstage = 60.0f;
constexpr float reference_speed = 48.0f;

} // namespace

auto Pedestrians::Initialize(float width, atom::world2d::AnimationLibrary2D& clips) -> bool {
    width_ = width;
    for (std::size_t row = 0; row < states_.size(); ++row) {
        auto& state = states_[row];
        state = "ped-" + std::to_string(row);
        atom::world2d::AnimationClip2D clip{.name = state, .texture_uri = std::string(pedestrian_uri)};
        for (int frame = 0; frame < frame_count; ++frame)
            clip.frames.push_back({.source = {static_cast<float>(frame_width * frame),
                                              static_cast<float>(frame_height * row), frame_width, frame_height},
                                   .duration_seconds = 0.12f});
        if (!clips.AddClip(std::move(clip)) || !clips.BindState(state, state))
            return false;
    }
    for (auto& group : groups_) {
        for (auto& entity : group.members) {
            entity = world_.CreateEntity();
            if (!SetUpSprite(world_, entity, {},
                             {.pivot = {0.5f, 1.0f},
                              .filter = atom::ecs::SpriteFilter::Nearest,
                              .layer = pedestrian_layer,
                              .visible = false}) ||
                !world_.Emplace<atom::ecs::Animator2D>(entity))
                return false;
        }
    }
    return true;
}

auto Pedestrians::Hide(Group& group) -> void {
    for (std::size_t i = 0; i < group.count; ++i) {
        world_.TryGet<atom::ecs::SpriteRenderer>(group.members[i])->visible = false;
        world_.TryGet<atom::ecs::Animator2D>(group.members[i])->playing = false;
    }
    group.count = 0;
}

auto Pedestrians::Place(const Group& group, bool snap) -> void {
    for (std::size_t i = 0; i < group.count; ++i) {
        const float direction = group.velocity > 0 ? 1.0f : -1.0f;
        const atom::algo::Vec2 position{group.x - direction * group.spacing * i, group.feet + 3.0f * i};
        const auto entity = group.members[i];
        world_.TryGet<atom::ecs::Transform2D>(entity)->position = position;
        if (snap)
            world_.TryGet<atom::ecs::RenderTransform2D>(entity)->position = position;
    }
}

auto Pedestrians::Spawn(Group& group, float x, int direction, const atom::world2d::AnimationSystem2D& animation)
    -> void {
    const bool pair = rng_.Below(10) < (street_ ? 4u : 3u);
    group.count = pair ? 2 : 1;
    group.x = x;
    group.feet = 213.0f + 3.0f * rng_.Below(4);
    group.velocity = direction * rng_.UniformFloat(33.0f, 56.0f);
    group.spacing = rng_.UniformFloat(21.0f, 31.0f);
    const auto first_style = rng_.Below(outfit_count);
    for (std::size_t i = 0; i < group.count; ++i) {
        const auto style = i == 0 ? first_style : (first_style < 2 ? 2u : 0u) + rng_.Below(2);
        const auto entity = group.members[i];
        (void)animation.SetState(world_, entity, states_[style * color_count + rng_.Below(color_count)]);
        auto& animator = *world_.TryGet<atom::ecs::Animator2D>(entity);
        animator.playing = true;
        animator.frame = static_cast<std::uint32_t>(rng_.Below(frame_count));
        animator.elapsed = 0.0f;
        animator.speed = std::abs(group.velocity) / reference_speed;
        auto& sprite = *world_.TryGet<atom::ecs::SpriteRenderer>(entity);
        sprite.visible = true;
        sprite.flip_x = direction < 0;
    }
    Place(group, true);
}

auto Pedestrians::Reset(bool street, const atom::world2d::AnimationSystem2D& animation) -> void {
    street_ = street;
    for (auto& group : groups_)
        Hide(group);
    const std::size_t initial_count = street_ ? 3 : 2;
    for (std::size_t i = 0; i < initial_count; ++i) {
        const float x = width_ * static_cast<float>(i + 1) / static_cast<float>(initial_count + 1) +
                        rng_.UniformFloat(-35.0f, 35.0f);
        Spawn(groups_[i], x, rng_.Below(2) == 0 ? -1 : 1, animation);
    }
    next_spawn_ = rng_.UniformFloat(3.0f, 6.0f);
}

auto Pedestrians::Step(float seconds, const atom::world2d::AnimationSystem2D& animation) -> void {
    for (auto& group : groups_) {
        if (group.count == 0)
            continue;
        group.x += group.velocity * seconds;
        if (group.x < -offstage - group.spacing || group.x > width_ + offstage + group.spacing)
            Hide(group);
        else
            Place(group, false);
    }
    next_spawn_ -= seconds;
    if (next_spawn_ > 0)
        return;
    const auto free = std::ranges::find_if(groups_, [](const Group& group) { return group.count == 0; });
    if (free == groups_.end())
        return;
    const int direction = rng_.Below(2) == 0 ? -1 : 1;
    Spawn(*free, direction > 0 ? -offstage : width_ + offstage, direction, animation);
    next_spawn_ = street_ ? rng_.UniformFloat(3.5f, 6.5f) : rng_.UniformFloat(5.0f, 9.0f);
}

auto Pedestrians::Count() const -> std::size_t {
    std::size_t count = 0;
    for (const auto& group : groups_)
        count += group.count;
    return count;
}

} // namespace homeless

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <World2D/Animation2D.hpp>

#include <cmath>
#include <utility>

#include <ECS/Components.hpp>

namespace atom::world2d {

auto AnimationLibrary2D::AddClip(AnimationClip2D clip) -> bool {
    if (clip.name.empty() || clip.texture_uri.empty() || clip.frames.empty() || clips_.contains(clip.name))
        return false;
    for (const auto& frame : clip.frames) {
        if (!std::isfinite(frame.duration_seconds) || frame.duration_seconds <= 0.0f ||
            !std::isfinite(frame.source.x) || !std::isfinite(frame.source.y) ||
            !std::isfinite(frame.source.width) || !std::isfinite(frame.source.height) ||
            frame.source.width <= 0.0f || frame.source.height <= 0.0f)
            return false;
    }
    clips_.emplace(clip.name, std::move(clip));
    return true;
}

auto AnimationLibrary2D::BindState(std::string state, std::string clip_name) -> bool {
    if (state.empty() || !clips_.contains(clip_name))
        return false;
    states_.insert_or_assign(std::move(state), std::move(clip_name));
    return true;
}

auto AnimationLibrary2D::FindState(const std::string_view state) const -> const AnimationClip2D* {
    const auto binding = states_.find(std::string{state});
    if (binding == states_.end())
        return nullptr;
    const auto clip = clips_.find(binding->second);
    return clip == clips_.end() ? nullptr : &clip->second;
}

auto AnimationLibrary2D::AddTransition(std::string from, std::string trigger, std::string to) -> bool {
    if (trigger.empty() || !states_.contains(from) || !states_.contains(to))
        return false;
    return transitions_[std::move(from)].emplace(std::move(trigger), std::move(to)).second;
}

auto AnimationLibrary2D::FindTransition(const std::string_view from,
                                        const std::string_view trigger) const -> std::string_view {
    const auto state = transitions_.find(std::string{from});
    if (state == transitions_.end())
        return {};
    const auto next = state->second.find(std::string{trigger});
    return next == state->second.end() ? std::string_view{} : next->second;
}

auto AnimationSystem2D::SetState(ecs::EntityWorld& world, const ecs::Entity entity,
                                 const std::string_view state) const -> bool {
    if (library_ == nullptr || library_->FindState(state) == nullptr)
        return false;
    auto* animator = world.TryGet<ecs::Animator2D>(entity);
    if (animator == nullptr)
        return false;
    if (animator->state == state)
        return true;
    animator->state = state;
    animator->elapsed = 0.0f;
    animator->frame = 0;
    animator->finished = false;
    animator->playing = true;
    return true;
}

auto AnimationSystem2D::Trigger(ecs::EntityWorld& world, const ecs::Entity entity,
                                const std::string_view trigger) const -> bool {
    const auto* animator = world.TryGet<ecs::Animator2D>(entity);
    if (animator == nullptr || library_ == nullptr)
        return false;
    const auto next = library_->FindTransition(animator->state, trigger);
    return !next.empty() && SetState(world, entity, next);
}

auto AnimationSystem2D::Step(ecs::EntityWorld& world, const float delta_seconds)
    -> const std::vector<AnimationEvent2D>& {
    events_.clear();
    if (library_ == nullptr || !std::isfinite(delta_seconds) || delta_seconds < 0.0f)
        return events_;
    for (const auto entity : world.With<ecs::Animator2D, ecs::SpriteRenderer>()) {
        auto* animator = world.TryGet<ecs::Animator2D>(entity);
        auto* sprite = world.TryGet<ecs::SpriteRenderer>(entity);
        if (animator == nullptr || sprite == nullptr)
            continue;
        const auto* clip = library_->FindState(animator->state);
        if (clip == nullptr)
            continue;
        if (animator->frame >= clip->frames.size()) {
            animator->frame = 0;
            animator->elapsed = 0.0f;
            animator->finished = false;
        }
        if (animator->playing && std::isfinite(animator->speed) && animator->speed > 0.0f) {
            animator->elapsed += delta_seconds * animator->speed;
            // Runtime caps fixed steps per display frame. This additional guard
            // prevents corrupt/hostile dt from flooding an event queue forever.
            std::size_t advanced = 0;
            while (advanced < 4096 && animator->elapsed >= clip->frames[animator->frame].duration_seconds) {
                animator->elapsed -= clip->frames[animator->frame].duration_seconds;
                ++advanced;
                if (animator->frame + 1 >= clip->frames.size()) {
                    if (!animator->loop) {
                        animator->frame = static_cast<std::uint32_t>(clip->frames.size() - 1);
                        animator->elapsed = 0.0f;
                        animator->playing = false;
                        animator->finished = true;
                        break;
                    }
                    animator->frame = 0;
                } else {
                    ++animator->frame;
                }
                const auto event_type = clip->frames[animator->frame].event_type;
                if (event_type != 0)
                    events_.push_back(AnimationEvent2D{entity, event_type});
            }
            if (advanced == 4096)
                animator->elapsed = std::fmod(animator->elapsed, clip->frames[animator->frame].duration_seconds);
        }
        const auto& frame = clip->frames[animator->frame];
        sprite->texture_uri = clip->texture_uri;
        sprite->source_position = {frame.source.x, frame.source.y};
        sprite->source_size = {frame.source.width, frame.source.height};
    }
    return events_;
}

} // namespace atom::world2d

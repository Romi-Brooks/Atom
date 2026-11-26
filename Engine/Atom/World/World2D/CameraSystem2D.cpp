// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Atom/World/World2D/CameraSystem2D.hpp>

#include <algorithm>
#include <cmath>

#include <Atom/World/ECS/Components.hpp>

namespace atom::world2d {

namespace {

auto FindTarget(ecs::EntityWorld& world, const std::uint32_t id) -> const ecs::Transform2D* {
    for (const auto entity : world.With<ecs::SceneEntityId>()) {
        const auto* scene_id = world.TryGet<ecs::SceneEntityId>(entity);
        if (scene_id != nullptr && scene_id->value == id)
            return world.TryGet<ecs::Transform2D>(entity);
    }
    return nullptr;
}

auto ClampAxis(const float value, const float lower, const float upper, const float half_view) -> float {
    if (lower > upper || !std::isfinite(lower) || !std::isfinite(upper))
        return value;
    if (upper - lower <= half_view * 2.0f)
        return (lower + upper) * 0.5f;
    return std::clamp(value, lower + half_view, upper - half_view);
}

} // namespace

auto CameraSystem2D::Step(ecs::EntityWorld& world, const algo::Vec2 output_viewport,
                           const float delta_seconds) -> std::size_t {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f)
        return 0;
    std::size_t updated = 0;
    for (const auto entity : world.With<ecs::Transform2D, ecs::Camera2D, ecs::CameraRig2D>()) {
        auto* transform = world.TryGet<ecs::Transform2D>(entity);
        const auto* lens = world.TryGet<ecs::Camera2D>(entity);
        auto* rig = world.TryGet<ecs::CameraRig2D>(entity);
        if (transform == nullptr || lens == nullptr || rig == nullptr)
            continue;
        if (!rig->initialized) {
            rig->base_position = transform->position;
            rig->initialized = true;
        }
        algo::Vec2 desired = rig->base_position;
        if (rig->target_scene_id != 0) {
            if (const auto* target = FindTarget(world, rig->target_scene_id); target != nullptr)
                desired = {target->position.GetX() + rig->look_ahead.GetX(),
                           target->position.GetY() + rig->look_ahead.GetY()};
        }
        const float dead_x = std::max(0.0f, rig->dead_zone.GetX() * 0.5f);
        const float dead_y = std::max(0.0f, rig->dead_zone.GetY() * 0.5f);
        const float dx = desired.GetX() - rig->base_position.GetX();
        const float dy = desired.GetY() - rig->base_position.GetY();
        desired = {rig->base_position.GetX() + (dx > dead_x ? dx - dead_x : dx < -dead_x ? dx + dead_x : 0.0f),
                   rig->base_position.GetY() + (dy > dead_y ? dy - dead_y : dy < -dead_y ? dy + dead_y : 0.0f)};
        const float half_life = rig->follow_half_life;
        const float weight = !std::isfinite(half_life) || half_life <= 0.0f
                                 ? 1.0f
                                 : 1.0f - std::exp2(-delta_seconds / half_life);
        rig->base_position = algo::Vec2::Lerp(rig->base_position, desired, weight);

        if (rig->panning) {
            rig->pan_elapsed = std::min(rig->pan_duration, rig->pan_elapsed + delta_seconds);
            const float t = rig->pan_elapsed / rig->pan_duration;
            const float eased = t * t * (3.0f - 2.0f * t);
            rig->base_position = algo::Vec2::Lerp(rig->pan_start, rig->pan_target, eased);
            if (rig->pan_elapsed >= rig->pan_duration)
                rig->panning = false;
        }

        if (rig->clamp_to_bounds) {
            const auto viewport = lens->viewport_size.GetX() > 0.0f && lens->viewport_size.GetY() > 0.0f
                                      ? lens->viewport_size
                                      : output_viewport;
            const float zoom = lens->zoom > 0.0f && std::isfinite(lens->zoom) ? lens->zoom : 1.0f;
            rig->base_position = {
                ClampAxis(rig->base_position.GetX(), rig->bounds_min.GetX(), rig->bounds_max.GetX(),
                          viewport.GetX() * 0.5f / zoom),
                ClampAxis(rig->base_position.GetY(), rig->bounds_min.GetY(), rig->bounds_max.GetY(),
                          viewport.GetY() * 0.5f / zoom)};
        }

        algo::Vec2 offset{};
        if (rig->shake_remaining > 0.0f && rig->shake_duration > 0.0f && rig->shake_amplitude > 0.0f) {
            rig->shake_remaining = std::max(0.0f, rig->shake_remaining - delta_seconds);
            const float elapsed = rig->shake_duration - rig->shake_remaining;
            const float decay = rig->shake_remaining / rig->shake_duration;
            offset = {std::sin(elapsed * 73.0f) * rig->shake_amplitude * decay,
                      std::sin(elapsed * 97.0f + 1.7f) * rig->shake_amplitude * decay};
        }
        transform->position = {rig->base_position.GetX() + offset.GetX(), rig->base_position.GetY() + offset.GetY()};
        ++updated;
    }
    return updated;
}

auto CameraSystem2D::Shake(ecs::EntityWorld& world, const ecs::Entity camera, const float amplitude,
                            const float duration_seconds) -> bool {
    if (!std::isfinite(amplitude) || amplitude < 0.0f || !std::isfinite(duration_seconds) ||
        duration_seconds <= 0.0f)
        return false;
    auto* rig = world.TryGet<ecs::CameraRig2D>(camera);
    if (rig == nullptr)
        return false;
    rig->shake_amplitude = amplitude;
    rig->shake_duration = duration_seconds;
    rig->shake_remaining = duration_seconds;
    return true;
}

auto CameraSystem2D::PanTo(ecs::EntityWorld& world, const ecs::Entity camera,
                           const algo::Vec2 target, const float duration_seconds) -> bool {
    if (!std::isfinite(target.GetX()) || !std::isfinite(target.GetY()) ||
        !std::isfinite(duration_seconds) || duration_seconds <= 0.0f)
        return false;
    auto* rig = world.TryGet<ecs::CameraRig2D>(camera);
    auto* transform = world.TryGet<ecs::Transform2D>(camera);
    if (rig == nullptr || transform == nullptr)
        return false;
    if (!rig->initialized) {
        rig->base_position = transform->position;
        rig->initialized = true;
    }
    rig->target_scene_id = 0;
    rig->pan_start = rig->base_position;
    rig->pan_target = target;
    rig->pan_duration = duration_seconds;
    rig->pan_elapsed = 0.0f;
    rig->panning = true;
    return true;
}

} // namespace atom::world2d

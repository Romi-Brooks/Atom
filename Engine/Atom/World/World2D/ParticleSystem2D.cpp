// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Atom/World/World2D/ParticleSystem2D.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <Atom/World/ECS/Components.hpp>

namespace atom::world2d {

namespace {

[[nodiscard]] auto FiniteNonNegative(const float value) -> bool {
    return std::isfinite(value) && value >= 0.0f;
}

} // namespace

ParticleSystem2D::ParticleSystem2D(const std::size_t capacity, const std::uint32_t random_seed)
    : capacity_(capacity), rng_(random_seed) {
    particles_.reserve(capacity_);
    draws_.reserve(capacity_);
}

auto ParticleSystem2D::AddPreset(ParticlePreset2D preset) -> bool {
    if (preset.name.empty() || preset.texture_uri.empty() || preset_index_.contains(preset.name) ||
        !FiniteNonNegative(preset.spawn_rate) || !std::isfinite(preset.lifetime_min) || preset.lifetime_min <= 0.0f ||
        !std::isfinite(preset.lifetime_max) || preset.lifetime_max < preset.lifetime_min ||
        !FiniteNonNegative(preset.speed_min) || !std::isfinite(preset.speed_max) ||
        preset.speed_max < preset.speed_min || !std::isfinite(preset.angle_min_radians) ||
        !std::isfinite(preset.angle_max_radians) || !std::isfinite(preset.acceleration.GetX()) ||
        !std::isfinite(preset.acceleration.GetY()) || !FiniteNonNegative(preset.size_start) ||
        !FiniteNonNegative(preset.size_end) || !std::isfinite(preset.source.x) || !std::isfinite(preset.source.y) ||
        !std::isfinite(preset.source.width) || !std::isfinite(preset.source.height) || preset.source.width <= 0.0f ||
        preset.source.height <= 0.0f)
        return false;
    const auto index = presets_.size();
    preset_index_.emplace(preset.name, index);
    presets_.push_back(std::move(preset));
    return true;
}

auto ParticleSystem2D::Spawn(const std::size_t preset_index, const algo::Vec2 position) -> bool {
    if (preset_index >= presets_.size() || particles_.size() >= capacity_)
        return false;
    const auto& preset = presets_[preset_index];
    const float lifetime = rng_.UniformFloat(preset.lifetime_min, preset.lifetime_max);
    const float speed = rng_.UniformFloat(preset.speed_min, preset.speed_max);
    const float angle = rng_.UniformFloat(std::min(preset.angle_min_radians, preset.angle_max_radians),
                                          std::max(preset.angle_min_radians, preset.angle_max_radians));
    particles_.push_back(Particle2D{.position = position,
                                    .velocity = {std::cos(angle) * speed, std::sin(angle) * speed},
                                    .acceleration = preset.acceleration,
                                    .age = 0.0f,
                                    .lifetime = lifetime,
                                    .size_start = preset.size_start,
                                    .size_end = preset.size_end,
                                    .preset_index = preset_index});
    return true;
}

auto ParticleSystem2D::EntityKey(const ecs::Entity entity) -> std::uint64_t {
    return (static_cast<std::uint64_t>(entity.Generation()) << 32U) | entity.Index();
}

auto ParticleSystem2D::Advance(const float delta_seconds) -> void {
    if (!FiniteNonNegative(delta_seconds))
        return;
    for (auto& particle : particles_) {
        particle.age += delta_seconds;
        particle.velocity = {particle.velocity.GetX() + particle.acceleration.GetX() * delta_seconds,
                             particle.velocity.GetY() + particle.acceleration.GetY() * delta_seconds};
        particle.position = {particle.position.GetX() + particle.velocity.GetX() * delta_seconds,
                             particle.position.GetY() + particle.velocity.GetY() * delta_seconds};
    }
    std::erase_if(particles_, [](const Particle2D& particle) { return particle.age >= particle.lifetime; });
}

auto ParticleSystem2D::Step(ecs::EntityWorld& world, const std::uint64_t world_generation, const float delta_seconds)
    -> std::size_t {
    if (!FiniteNonNegative(delta_seconds))
        return 0;
    if (has_generation_ && world_generation_ != world_generation)
        Clear();
    world_generation_ = world_generation;
    has_generation_ = true;
    Advance(delta_seconds);

    std::unordered_set<std::uint64_t> live_emitters{};
    std::size_t spawned = 0;
    for (const auto entity : world.With<ecs::Transform2D, ecs::ParticleEmitter2D>()) {
        const auto* transform = world.TryGet<ecs::Transform2D>(entity);
        const auto* emitter = world.TryGet<ecs::ParticleEmitter2D>(entity);
        if (transform == nullptr || emitter == nullptr)
            continue;
        const auto key = EntityKey(entity);
        live_emitters.insert(key);
        if (!emitter->enabled || !FiniteNonNegative(emitter->rate_multiplier))
            continue;
        const auto found = preset_index_.find(emitter->preset);
        if (found == preset_index_.end())
            continue;
        auto& remainder = emission_remainder_[key];
        const double desired =
            static_cast<double>(remainder) +
            static_cast<double>(presets_[found->second].spawn_rate) * emitter->rate_multiplier * delta_seconds;
        // Excess emission is dropped when the pool is full. Do not accrue an
        // unbounded debt that causes a burst minutes after the scene recovers.
        const auto requested = static_cast<std::size_t>(std::min(std::floor(desired), 4096.0));
        remainder = std::isfinite(desired) ? static_cast<float>(desired - std::floor(desired)) : 0.0f;
        for (std::size_t i = 0; i < requested; ++i) {
            if (!Spawn(found->second, transform->position))
                break;
            ++spawned;
        }
    }
    for (auto it = emission_remainder_.begin(); it != emission_remainder_.end();) {
        if (!live_emitters.contains(it->first))
            it = emission_remainder_.erase(it);
        else
            ++it;
    }
    return spawned;
}

auto ParticleSystem2D::Burst(const std::string_view preset, const algo::Vec2 position, const std::size_t count)
    -> std::size_t {
    const auto found = preset_index_.find(std::string{preset});
    if (found == preset_index_.end() || !std::isfinite(position.GetX()) || !std::isfinite(position.GetY()))
        return 0;
    std::size_t spawned = 0;
    for (; spawned < count && Spawn(found->second, position); ++spawned) {
    }
    return spawned;
}

auto ParticleSystem2D::Collect(const bool sort_by_layer) -> const std::vector<ParticleDraw2D>& {
    draws_.clear();
    for (const auto& particle : particles_) {
        const auto& preset = presets_[particle.preset_index];
        const float progress = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
        const float size = particle.size_start + (particle.size_end - particle.size_start) * progress;
        if (size <= 0.0f)
            continue;
        const auto alpha = static_cast<std::uint8_t>(static_cast<float>(preset.tint.a) * (1.0f - progress));
        auto tint = preset.tint;
        tint.a = alpha;
        draws_.push_back(ParticleDraw2D{
            .preset_index = particle.preset_index,
            .source = preset.source,
            .destination = {particle.position.GetX() - size * 0.5f, particle.position.GetY() - size * 0.5f, size, size},
            .tint = tint,
            .layer = preset.layer,
            .filter = preset.filter});
    }
    if (sort_by_layer) {
        std::stable_sort(draws_.begin(), draws_.end(),
                         [](const ParticleDraw2D& a, const ParticleDraw2D& b) { return a.layer < b.layer; });
    }
    return draws_;
}

auto ParticleSystem2D::PresetTextureUri(const std::size_t preset_index) const -> std::string_view {
    if (preset_index >= presets_.size())
        return {};
    return presets_[preset_index].texture_uri;
}

auto ParticleSystem2D::Clear() -> void {
    particles_.clear();
    draws_.clear();
    emission_remainder_.clear();
}

} // namespace atom::world2d

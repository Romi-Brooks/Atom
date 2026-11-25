// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file ParticleSystem2D.hpp
 * @brief Bounded 2D particle simulation with ECS-attached emitters.
 */
#ifndef ATOM_WORLD2D_PARTICLESYSTEM2D_HPP
#define ATOM_WORLD2D_PARTICLESYSTEM2D_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Atom/Core/Algorithm/Color/Color.hpp>
#include <Atom/Core/Algorithm/Geometry/Rect.hpp>
#include <Atom/Core/Algorithm/Vector/Vec2.hpp>
#include <Atom/World/ECS/Components.hpp>
#include <Atom/World/ECS/EntityWorld.hpp>
#include <Atom/Core/Utilities/Random/Rng.hpp>

namespace atom::world2d {

struct ParticlePreset2D final {
        std::string name{};
        std::string texture_uri{};
        algo::Rect source{};
        float spawn_rate = 10.0f; // particles / second
        float lifetime_min = 0.5f;
        float lifetime_max = 1.0f;
        float speed_min = 0.0f;
        float speed_max = 50.0f;
        float angle_min_radians = 0.0f;
        float angle_max_radians = 6.2831853f;
        algo::Vec2 acceleration{};
        float size_start = 8.0f;
        float size_end = 0.0f;
        color::Color tint = color::Color::White();
        std::int32_t layer = 0;
        ecs::SpriteFilter filter = ecs::SpriteFilter::Linear;
};

struct Particle2D final {
        algo::Vec2 position{};
        algo::Vec2 velocity{};
        algo::Vec2 acceleration{};
        float age = 0.0f;
        float lifetime = 1.0f;
        float size_start = 1.0f;
        float size_end = 1.0f;
        std::size_t preset_index = 0;
};

struct ParticleDraw2D final {
        // Index into the preset table, not a copy of the preset's URI: Collect()
        // runs every frame for every live particle, and copying the URI string
        // there was a heap allocation per particle per frame. The owner resolves
        // it once per draw batch through PresetTextureUri().
        std::size_t preset_index = 0;
        algo::Rect source{};
        algo::Rect destination{};
        color::Color tint{};
        std::int32_t layer = 0;
        ecs::SpriteFilter filter = ecs::SpriteFilter::Linear;
};

class ParticleSystem2D final {
    public:
        explicit ParticleSystem2D(std::size_t capacity = 16384, std::uint32_t random_seed = 1);

        [[nodiscard]] auto AddPreset(ParticlePreset2D preset) -> bool;
        // Integrate and expire live particles without an ECS world.
        auto Advance(float delta_seconds) -> void;
        // Clears the simulation when GameWorld::GetWorldGeneration changes.
        // Call once per fixed step after entity transforms are updated.
        [[nodiscard]] auto Step(ecs::EntityWorld& world, std::uint64_t world_generation, float delta_seconds)
            -> std::size_t;
        // A one-shot effect can be emitted without an ECS emitter. Returns the
        // number admitted by the capacity budget.
        [[nodiscard]] auto Burst(std::string_view preset, algo::Vec2 position, std::size_t count) -> std::size_t;
        // Collect in stable layer order, or in simulation order for callers
        // that will sort a combined draw list.
        [[nodiscard]] auto Collect(bool sort_by_layer = true) -> const std::vector<ParticleDraw2D>&;
        // Texture URI of a registered preset, for the draw collector. Empty when
        // `preset_index` is out of range (a stale draw list).
        [[nodiscard]] auto PresetTextureUri(std::size_t preset_index) const -> std::string_view;
        [[nodiscard]] auto Count() const noexcept -> std::size_t {
            return particles_.size();
        }
        [[nodiscard]] auto Capacity() const noexcept -> std::size_t {
            return capacity_;
        }
        auto Clear() -> void;

    private:
        [[nodiscard]] auto Spawn(std::size_t preset_index, algo::Vec2 position) -> bool;
        [[nodiscard]] static auto EntityKey(ecs::Entity entity) -> std::uint64_t;

        std::size_t capacity_ = 0;
        random::Rng rng_;
        std::uint64_t world_generation_ = 0;
        bool has_generation_ = false;
        std::vector<ParticlePreset2D> presets_{};
        std::unordered_map<std::string, std::size_t> preset_index_{};
        std::unordered_map<std::uint64_t, float> emission_remainder_{};
        std::vector<Particle2D> particles_{};
        std::vector<ParticleDraw2D> draws_{};
};

} // namespace atom::world2d

#endif // ATOM_WORLD2D_PARTICLESYSTEM2D_HPP

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file SceneTypes.hpp
 * @brief The validated, engine-side shape of a scene document (T2.1 format).
 *
 * These are plain values with no JSON, no ECS and no filesystem in sight: the
 * document is parsed into them once, and both the world builder (SceneLoader) and
 * any future tool work from them. Keeping the format's data separate from the DOM
 * is what stops parser types from leaking into the rest of the engine
 * (T2.1 acceptance: "the chosen parser must not leak into ECS/Render public API").
 *
 * Units are the document's units: world pixels, y down, radians CCW. Physics
 * converts once at its own boundary (T3.1), not here.
 */

#ifndef ATOM_SCENE_SCENETYPES_HPP
#define ATOM_SCENE_SCENETYPES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <Utilities/Json/Diagnostic.hpp>

#include <Algorithm/Color/Color.hpp>
#include <ECS/Components.hpp>
#include <Algorithm/Geometry/Rect.hpp>
#include <Algorithm/Vector/Vec2.hpp>

namespace atom::scene {

// Current document version.
//
// v2 added the optional `physics` block (scene gravity / pixels-per-metre, and a per-entity
// body). It is *additive*: a v1 document is still valid and is read as "no physics", so the
// loader accepts exactly {1, 2} rather than "anything it can parse" - an unknown version is
// still rejected outright.
inline constexpr std::uint32_t kSchemaVersion = 2;
inline constexpr std::uint32_t kOldestSupportedSchemaVersion = 1;

// "no active camera" marker for SceneDesc::active_camera.
inline constexpr std::size_t kNoActiveCamera = static_cast<std::size_t>(-1);

struct TransformDesc {
        algo::Vec2 position{};
        float rotation = 0.0F;
        algo::Vec2 scale{1.0F, 1.0F};
};

struct SpriteDesc {
        std::string texture_uri{};
        bool has_source = false;
        algo::Rect source{};
        bool has_size = false;
        algo::Vec2 size{};
        algo::Vec2 pivot{0.5F, 0.5F};
        color::Color tint = color::Color::White();
        // Filled from the atlas when the sprite names a frame; Linear for a bare
        // texture. Sprite rendering maps it onto the renderer's filter.
        ecs::SpriteFilter filter = ecs::SpriteFilter::Linear;
        std::int32_t layer = 0;
        std::int32_t sort_key = 0;
        bool flip_x = false;
        bool flip_y = false;
};

struct CameraDesc {
        float zoom = 1.0F;
        algo::Vec2 viewport{};
        bool active = false;
};

// Physics body shell for one entity (v2). Sizes are in PIXELS, like every other size in the
// document; the loader divides by the scene's pixels-per-metre so the scene file stays in one
// unit system and the conversion happens exactly once.
struct PhysicsDesc {
        enum class Kind : std::uint8_t { Static, Kinematic, Dynamic };

        Kind kind = Kind::Static;
        // radius > 0 selects a circle; otherwise half_extent describes a box (both in pixels).
        float radius = 0.0F;
        algo::Vec2 half_extent{25.0F, 25.0F};
        float density = 1.0F;
        float friction = 0.5F;
        float restitution = 0.0F;
        bool is_sensor = false;
};

struct EntityDesc {
        std::uint32_t id = 0;
        bool has_name = false;
        std::string name{};
        bool active = false;
        bool is_static = false;
        TransformDesc transform{};
        std::optional<SpriteDesc> sprite{};
        std::optional<CameraDesc> camera{};
        std::optional<float> lifetime{};
        std::optional<PhysicsDesc> physics{};
        // Where the entity starts in the document, so a later build failure can
        // still point at the right place.
        std::uint32_t line = 0;
        std::uint32_t column = 0;
};

struct SceneDesc {
        std::uint32_t schema_version = kSchemaVersion;
        // v2: physics scale and gravity. Defaults keep a v1 document meaningful, and the
        // integration layer is created with the same pixels-per-metre the loader used.
        float pixels_per_meter = 100.0F;
        algo::Vec2 gravity{0.0F, -9.81F}; // metres/second^2, physics axes (y up)
        std::string id{};
        bool has_name = false;
        std::string name{};
        std::vector<EntityDesc> entities{};
        // Index into `entities`, or kNoActiveCamera.
        std::size_t active_camera = kNoActiveCamera;
};

// One validation problem, shared with every other document format so that a scene, a
// prefab and a sprite atlas all complain in the same shape:
//   res://scenes/x.scene.json: entity 7 ("crate"): sprite.pivot: expected 2 numbers
using Diagnostic = json::Diagnostic;

} // namespace atom::scene

#endif // ATOM_SCENE_SCENETYPES_HPP

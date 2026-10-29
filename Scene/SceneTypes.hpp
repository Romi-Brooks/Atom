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

// Current document version. A document with any other value is rejected.
inline constexpr std::uint32_t kSchemaVersion = 1;

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
        // Where the entity starts in the document, so a later build failure can
        // still point at the right place.
        std::uint32_t line = 0;
        std::uint32_t column = 0;
};

struct SceneDesc {
        std::uint32_t schema_version = kSchemaVersion;
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

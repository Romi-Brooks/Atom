/**
 * @file           : Components.hpp
 * @brief          : The engine's component vocabulary, and the component list that
 *                   drives EntityWorld's explicit instantiations.
 *
 * Two things live here on purpose:
 *
 *  1. The component types themselves. They are plain data: no virtual functions,
 *     no back-pointers to the world, no behaviour. Systems own behaviour.
 *  2. ATOM_ECS_COMPONENTS - the single list of component types in the engine.
 *     EntityWorld.cpp expands it to instantiate its backend templates, so adding
 *     a component is: define it here, add it to the list below, done.
 *
 * Keeping the list explicit (rather than auto-registering on first use) is what
 * lets the backend stay out of every public header: the instantiations happen in
 * one .cpp, and a translation unit that merely includes EntityWorld.hpp compiles
 * no backend templates at all.
 *
 * ADMISSION RULES (A0.1, frozen - Docs/Plans/ECS-Component-Vocabulary-CN.md):
 * a type listed below must be
 *   1. pure data - no virtuals, no behaviour, no pointer back to the world;
 *   2. handle-free - no TextureHandle, Texture*, GPU or physics backend types;
 *   3. platform-free - no SDL/SDL_GPU/EnTT/third-party types;
 *   4. module-free - no AssetPath, ResourceId, IFile, Loader or Renderer types;
 *   5. default constructible, movable and destructible.
 * Resources are referenced by a stable string key (e.g. texture_uri) and resolved
 * by a system (Scene2D, Scene loader), never by the component itself. Allowed
 * value types come from Atom_Math / Atom_Color, the two leaf targets, so adding a
 * component never gives Atom_ECS a dependency on Render, Asset or Physics.
**/

#ifndef ATOM_ECS_COMPONENTS_HPP
#define ATOM_ECS_COMPONENTS_HPP

#include <cstdint>
#include <string>

#include <Algorithm/Color/Color.hpp>
#include <Algorithm/Vector/Vec2.hpp>

namespace atom::ecs {

// --- transform / motion -----------------------------------------------------

struct Position final {
        algo::Vec2 value{};
};

struct Velocity final {
        algo::Vec2 value{};
};

struct Rotation final {
        float radians = 0.0f;
};

struct Scale final {
        algo::Vec2 value{1.0f, 1.0f};
};

// --- identity ---------------------------------------------------------------

// Display name. Kept out of Entity on purpose: a name is optional data, and
// paying for a std::string on every entity would be wasteful.
struct Name final {
        std::string value{};
};

// --- gameplay ---------------------------------------------------------------

struct Health final {
        int32_t current = 0;
        int32_t maximum = 0;
};

// --- tags -------------------------------------------------------------------
//
// Empty types: presence is the whole datum. They carry no value, so the API
// refuses Patch() on them (see EntityWorld::Patch).

// Entity participates in simulation this frame.
struct Active final {};

// Entity is not moved by physics or animation.
struct Static final {};

// Entity should be destroyed at the end of the frame. A deferred-destroy tag
// rather than an immediate Destroy() call, so a system never structurally
// modifies the world it is iterating.
struct PendingDestroy final {};

// --- 2D scene vocabulary (T1.1) --------------------------------------------
//
// These six types are the frozen first batch of game/scene components. They are
// deliberately separate from the algo:: value types of the same idea:
//
//   - algo::Transform2D carries a pivot because it is a rendering transform;
//     ecs::Transform2D is simulation state and must not (pivot is sprite data).
//   - algo::Camera2D is the maths (view-projection matrix, screen<->world); the
//     component is only the lens, so ECS never stores a matrix.
//
// See Docs/Plans/ECS-Component-Vocabulary-CN.md for the admission rules.

// Simulation transform: metres/pixels agnostic, radians, counter-clockwise.
struct Transform2D final {
        algo::Vec2 position{};
        float rotation = 0.0f;
        algo::Vec2 scale{1.0f, 1.0f};
};

// The display copy of Transform2D. Written once per frame by the transform
// snapshot system and only *read* by the 2D render system, which is what keeps
// rendering from writing simulation coordinates.
//
// Interpolation between fixed steps is not modelled here on purpose: it needs a
// previous-state buffer and an alpha, which belong to the renderer's frame, not
// to the world. Until that lands, the display state is a plain copy.
struct RenderTransform2D final {
        algo::Vec2 position{};
        float rotation = 0.0f;
        algo::Vec2 scale{1.0f, 1.0f};
};

// How this sprite's texture is sampled. A plain value, so it belongs in ECS; the
// renderer maps it onto its own filter enum, because ECS may not include render
// contracts (admission rule R4).
enum class SpriteFilter : std::uint8_t { Linear, Nearest };

// A textured sprite. Pure data: the texture is named by a stable asset URI and
// resolved by the 2D scene layer, so ECS never sees a handle (admission rules
// R2/R4).
struct SpriteRenderer final {
        // Asset URI, e.g. "res://textures/player.png". Empty means the sprite has
        // not been resolved yet and draws nothing.
        std::string texture_uri{};

        // Source rectangle in texels. A zero size means "the whole texture".
        algo::Vec2 source_position{};
        algo::Vec2 source_size{};

        // World size in pixels. Zero means "use source_size", i.e. 1 texel = 1 px.
        algo::Vec2 size{};

        // Origin inside the sprite, normalized to [0,1]: (0.5,0.5) is the centre,
        // (0,0) the top-left corner.
        algo::Vec2 pivot{0.5f, 0.5f};

        color::Color tint = color::Color::White();

        // Pixel art must be sampled with Nearest; scaling an 8x8 tile up with Linear
        // turns crisp edges into mush. Defaults to Linear because that is the right
        // choice for anything that is not sprite art.
        SpriteFilter filter = SpriteFilter::Linear;

        // Draw order: layer first, then ascending sort_key, then entity index.
        std::int32_t layer = 0;
        std::int32_t sort_key = 0;

        // Mirroring. A sprite is the only place a flip belongs; the simulation
        // transform keeps facing independent of how the art is mirrored.
        bool flip_x = false;
        bool flip_y = false;
};

// A 2D camera. The eye position comes from the same entity's Transform2D; this
// component is only the lens. The active camera is the lowest-index entity that
// owns one, which keeps "one active camera" deterministic without a second tag.
struct Camera2D final {
        // Must be > 0; a non-positive value is rejected by the 2D scene layer.
        float zoom = 1.0f;
        // Zero means "the render target size".
        algo::Vec2 viewport_size{};
};

// Countdown in seconds. The owning system marks the entity PendingDestroy when
// this reaches zero; it never destroys the entity mid-iteration.
struct Lifetime final {
        float remaining_seconds = 0.0f;
};

// Stable per-scene entity id, for save data and scene round-trips. It is not an
// ECS identity: Entity remains the generation-checked handle.
struct SceneEntityId final {
        std::uint32_t value = 0;
};

// --- physics shell (T3.1b) -------------------------------------------------
//
// The *shell* lives here as pure data; the world, the bodies and the stepping live in
// `Physics/Integration/`. Physics/Integration declares no component of its own because a
// component type has to appear in ATOM_ECS_COMPONENTS, and moving the list out of ECS
// would be a bigger boundary change than this task justifies - so the shell is declared
// where every other component is, and it holds only numbers (never an IPhysicsBody*).

enum class PhysicsBodyKind : std::uint8_t {
    Static,    // terrain, walls: collides, never moves on its own
    Kinematic, // moved by code, pushes others, ignores forces
    Dynamic,   // full simulation
};

// One body with one shape. A separate collider component would be the natural home for
// multi-fixture bodies; until that exists, one body == one shape keeps the mapping honest.
//
// All sizes are in METRES, because that is what the physics world speaks. The conversion
// to and from pixels happens in exactly one place (Physics2DWorld), never here and never
// in a system.
struct PhysicsBody2D final {
        PhysicsBodyKind kind = PhysicsBodyKind::Static;

        // radius > 0 selects a circle; otherwise half_extent describes a box.
        float radius = 0.0F;
        algo::Vec2 half_extent{0.5F, 0.5F};

        float density = 1.0F;
        float friction = 0.5F;
        float restitution = 0.0F;

        // Reports contacts, never resolves them (see BodyDef::is_sensor).
        bool is_sensor = false;

        // Written by the integration layer: the engine-side body handle, 1-based so 0 can
        // mean "not created yet". A number, not a pointer, for the same reason user_data is.
        std::uint32_t handle = 0;
};

} // namespace atom::ecs

// The engine's component list. Every type here must be default constructible and
// movable. Order does not matter; the list exists so EntityWorld.cpp can
// instantiate its backend templates once.
#define ATOM_ECS_COMPONENTS                                                                                            \
    ::atom::ecs::Position, ::atom::ecs::Velocity, ::atom::ecs::Rotation, ::atom::ecs::Scale, ::atom::ecs::Name,        \
        ::atom::ecs::Health, ::atom::ecs::Active, ::atom::ecs::Static, ::atom::ecs::PendingDestroy,                    \
        ::atom::ecs::Transform2D, ::atom::ecs::RenderTransform2D, ::atom::ecs::SpriteRenderer,                         \
        ::atom::ecs::Camera2D, ::atom::ecs::Lifetime, ::atom::ecs::SceneEntityId, ::atom::ecs::PhysicsBody2D

#endif // ATOM_ECS_COMPONENTS_HPP

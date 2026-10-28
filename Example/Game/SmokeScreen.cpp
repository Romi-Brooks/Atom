#include "SmokeScreen.hpp"

#include <cmath>
#include <string>
#include <string_view>

#include <ECS/Components.hpp>
#include <Filesystem/Vfs.hpp>
#include <Input/InputSystem.hpp>
#include <Log/LogSystem.hpp>
#include <Runtime/System.hpp>

namespace atom::game {

namespace {

using ecs::Camera2D;
using ecs::Entity;
using ecs::EntityWorld;
using ecs::RenderTransform2D;
using ecs::SceneEntityId;
using ecs::SpriteRenderer;
using ecs::Transform2D;
using runtime::FrameContext;
using runtime::GameWorld;
using runtime::SystemPhase;

constexpr float kPlayerSpeed = 220.0F; // pixels per second
constexpr float kPlayerSize = 32.0F;
constexpr float kTileSize = 48.0F;
// Large enough to cover a 1280x720 window plus some camera travel, so the scene
// reads as a room rather than as a floating patch of floor.
constexpr int kFloorColumns = 40;
constexpr int kFloorRows = 25;
// The camera and the player both start here, so the whole window lands inside the
// floor instead of showing the void above and to the left of the world origin.
constexpr float kWorldCenterX = kTileSize * (static_cast<float>(kFloorColumns) * 0.5F);
constexpr float kWorldCenterY = kTileSize * (static_cast<float>(kFloorRows) * 0.5F);

auto MakeSprite(std::string uri, const float width, const float height) -> SpriteRenderer {
    SpriteRenderer sprite{};
    sprite.texture_uri = std::move(uri);
    sprite.size = {width, height};
    sprite.pivot = {0.5F, 0.5F};
    return sprite;
}

// Builds one sprite entity. The stable scene id is the caller's business: floor
// tiles do not need one, and keeping it out of here avoids an id-shaped parameter
// that the tile grid would have to invent values for.
auto SpawnSprite(EntityWorld& entities, std::string uri, const float x, const float y, const float width,
                 const float height, const std::int32_t layer, const std::int32_t sort_key) -> Entity {
    const auto entity = entities.CreateEntity();
    if (!entities.Emplace<Transform2D>(entity, Transform2D{.position = {x, y}}).has_value())
        return {};
    if (!entities.Emplace<RenderTransform2D>(entity).has_value())
        return {};
    auto sprite = MakeSprite(std::move(uri), width, height);
    sprite.layer = layer;
    sprite.sort_key = sort_key;
    if (!entities.Emplace<SpriteRenderer>(entity, sprite).has_value())
        return {};
    return entity;
}

} // namespace

auto SeedSmokeScene(GameWorld& world) -> Result<void> {
    auto& entities = world.GetEntities();
    auto& scheduler = world.GetScheduler();

    // Every entity here is created directly, not through the command buffer: this
    // runs during GameScreen construction, before any phase is iterating, so a
    // structural change is legal. Spawning from inside a phase (T2/T3) must queue.
    std::uint32_t next_id = 1;

    // --- camera -------------------------------------------------------------
    const auto camera = entities.CreateEntity();
    if (auto placed = entities.Emplace<Transform2D>(camera, Transform2D{.position = {kWorldCenterX, kWorldCenterY}});
        !placed.has_value())
        return placed;
    if (auto placed = entities.Emplace<Camera2D>(camera, Camera2D{.zoom = 1.0F}); !placed.has_value())
        return placed;
    if (auto placed = entities.Emplace<SceneEntityId>(camera, SceneEntityId{.value = next_id++}); !placed.has_value())
        return placed;

    // --- floor: a grid whose sort keys collide in pairs ---------------------
    //
    // The collision is intentional: two sprites with the same (layer, sort_key) must
    // still come out in entity order, which is what makes the draw order stable.
    for (auto row = 0; row < kFloorRows; ++row) {
        for (auto column = 0; column < kFloorColumns; ++column) {
            const auto x = kTileSize * (static_cast<float>(column) + 0.5F);
            const auto y = kTileSize * (static_cast<float>(row) + 0.5F);
            if (!SpawnSprite(entities, "res://textures/floor.png", x, y, kTileSize, kTileSize, /*layer=*/-10,
                             /*sort_key=*/row)
                     .IsValid())
                return Fail(MakeError(ErrorCode::Internal, Module::Core, "SeedSmokeScene")
                                .WithDetail("floor tile spawn failed"));
        }
    }

    // --- crates -------------------------------------------------------------
    const auto crate_left = SpawnSprite(entities, "res://textures/crate.png", kWorldCenterX - 160.0F, kWorldCenterY - 90.0F,
                                        kTileSize, kTileSize, /*layer=*/0, /*sort_key=*/10);
    const auto crate_right = SpawnSprite(entities, "res://textures/crate.png", kWorldCenterX + 190.0F, kWorldCenterY + 60.0F,
                                         kTileSize, kTileSize, /*layer=*/0, /*sort_key=*/20);
    if (!crate_left.IsValid() || !crate_right.IsValid())
        return Fail(MakeError(ErrorCode::Internal, Module::Core, "SeedSmokeScene").WithDetail("crate spawn failed"));

    // --- one atlas cell, to exercise the source-rect path -------------------
    {
        const auto entity = entities.CreateEntity();
        if (auto placed = entities.Emplace<Transform2D>(entity, Transform2D{.position = {kWorldCenterX - 300.0F, kWorldCenterY + 130.0F}});
            !placed.has_value())
            return placed;
        if (auto placed = entities.Emplace<RenderTransform2D>(entity); !placed.has_value())
            return placed;
        auto sprite = MakeSprite("res://textures/atlas.png", 64.0F, 64.0F);
        sprite.source_position = {8.0F, 0.0F};
        sprite.source_size = {8.0F, 8.0F};
        sprite.layer = 0;
        sprite.sort_key = 5;
        if (auto placed = entities.Emplace<SpriteRenderer>(entity, sprite); !placed.has_value())
            return placed;
        if (auto placed = entities.Emplace<SceneEntityId>(entity, SceneEntityId{.value = next_id++});
            !placed.has_value())
            return placed;
    }

    // --- player -------------------------------------------------------------
    const auto player = SpawnSprite(entities, "res://textures/player.png", kWorldCenterX, kWorldCenterY,
                                    kPlayerSize, kPlayerSize, /*layer=*/10, /*sort_key=*/100);
    if (!player.IsValid())
        return Fail(MakeError(ErrorCode::Internal, Module::Core, "SeedSmokeScene").WithDetail("player spawn failed"));
    if (auto placed = entities.Emplace<SceneEntityId>(player, SceneEntityId{.value = next_id++}); !placed.has_value())
        return placed;

    // --- systems ------------------------------------------------------------
    //
    // Captured by value: these run during phases, long after the screen exists, and
    // must not depend on screen members (which are not initialised while the base
    // GameScreen constructor runs).
    if (auto added = scheduler.Add(
            SystemPhase::FixedUpdate,
            runtime::SystemDescriptor{
                "move-player", [player](GameWorld& world_ref, const FrameContext& context) -> Result<void> {
                    const auto& input = input::InputSystem::GetInstance();
                    // Movement is continuous input: it stops only while a text field
                    // owns the keyboard.
                    if (input.IsTextInputCapturedByUi())
                        return Ok();

                    float direction_x = 0.0F;
                    float direction_y = 0.0F;
                    if (input.IsKeyHeld(input::Key::A) || input.IsKeyHeld(input::Key::Left))
                        direction_x -= 1.0F;
                    if (input.IsKeyHeld(input::Key::D) || input.IsKeyHeld(input::Key::Right))
                        direction_x += 1.0F;
                    if (input.IsKeyHeld(input::Key::W) || input.IsKeyHeld(input::Key::Up))
                        direction_y -= 1.0F;
                    if (input.IsKeyHeld(input::Key::S) || input.IsKeyHeld(input::Key::Down))
                        direction_y += 1.0F;
                    if (direction_x == 0.0F && direction_y == 0.0F)
                        return Ok();

                    // Normalise so a diagonal is not 1.41x faster.
                    const float length = std::sqrt(direction_x * direction_x + direction_y * direction_y);
                    direction_x /= length;
                    direction_y /= length;

                    auto& entities_ref = world_ref.GetEntities();
                    auto* transform = entities_ref.TryGet<Transform2D>(player);
                    if (transform == nullptr)
                        return Ok();
                    const float step = kPlayerSpeed * context.delta_seconds;
                    transform->position = {transform->position.GetX() + direction_x * step,
                                           transform->position.GetY() + direction_y * step};

                    // Facing is a sprite property, not a simulation one, so the flip
                    // lives next to the movement that causes it.
                    if (auto* sprite = entities_ref.TryGet<SpriteRenderer>(player); sprite != nullptr &&
                                                                                 direction_x != 0.0F) {
                        sprite->flip_x = direction_x < 0.0F;
                    }
                    return Ok();
                }});
        !added.has_value())
        return added;

    if (auto added = scheduler.Add(
            SystemPhase::LateUpdate,
            runtime::SystemDescriptor{
                "camera-follow", [player, camera](GameWorld& world_ref, const FrameContext& context) -> Result<void> {
                    auto& entities_ref = world_ref.GetEntities();
                    const auto* target = entities_ref.TryGet<Transform2D>(player);
                    auto* eye = entities_ref.TryGet<Transform2D>(camera);
                    if (target == nullptr || eye == nullptr)
                        return Ok();

                    // A quarter-second-smoothed follow: frame-rate independent because
                    // the blend uses the variable delta it is given.
                    const float blend = 1.0F - std::exp(-8.0F * context.delta_seconds);
                    eye->position = algo::Vec2::Lerp(eye->position, target->position, blend);
                    return Ok();
                }});
        !added.has_value())
        return added;

    return Ok();
}

SmokeScreen::SmokeScreen()
    : GameScreen([](GameWorld& world) -> Result<void> { return SeedSmokeScene(world); }) {}

SmokeScreen::~SmokeScreen() {
    // The GPU device outlives the screen, so renderer-owned resources must be
    // released before the device goes away.
    renderer_.Shutdown();
}

auto SmokeScreen::OnWorldFrameAdvanced(const float /*delta_seconds*/) -> void {
    // Simulation -> display copy. Render only ever reads RenderTransform2D, which is
    // why a dropped frame shows stale positions instead of tearing the world.
    render::scene2d::Scene2DRenderer::SnapshotTransforms(World().GetEntities());
}

auto SmokeScreen::Render(render::IRenderDevice& device) -> void {
    if (!renderer_.IsInitialized() && !renderer_.Initialize(device, ATOM_SHADER_OUTPUT_DIR)) {
        if (!render_failed_) {
            LOG_ERROR(atom::log::core::Screen, "2D smoke scene: Renderer2D initialization failed");
            render_failed_ = true;
        }
        return;
    }

    const auto size = device.GetOutputSize();
    window_width_ = static_cast<std::uint32_t>(size.GetX());
    window_height_ = static_cast<std::uint32_t>(size.GetY());

    // Both calls must happen outside Renderer2D::BeginFrame, which Submit() opens.
    texture_cache_.PumpAsync();
    texture_cache_.FlushDeferredDestroys();

    auto& entities = World().GetEntities();
    const auto view = render::scene2d::Scene2DRenderer::FindActiveCamera(
        entities, {static_cast<float>(window_width_), static_cast<float>(window_height_)});
    camera_zoom_ = view.zoom;

    scene_renderer_.Collect(entities);
    scene_renderer_.Submit(renderer_, view);
}

auto SmokeScreen::HandleEvent(const window::IEvent& event) -> bool {
    if (event.type != window::EventType::KeyPressed)
        return false;
    const auto& key = std::get<window::KeyEvent>(event.data);
    if (key.key != input::Key::Space)
        return false;

    // Space toggles the world without stopping the host: the screen keeps
    // rendering, so a paused scene is visibly paused.
    SetPaused(!IsPaused());
    return true;
}

} // namespace atom::game

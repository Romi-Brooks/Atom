#include "SmokeScreen.hpp"

#include <cmath>
#include <string>
#include <string_view>
#include <utility>

#include <ECS/Components.hpp>
#include <Input/InputSystem.hpp>
#include <Log/LogSystem.hpp>
#include <Runtime/System.hpp>
#include <Scene/SceneLoader.hpp>

namespace atom::game {

namespace {

using ecs::RenderTransform2D;
using ecs::SceneEntityId;
using ecs::SpriteRenderer;
using ecs::Transform2D;
using runtime::FrameContext;
using runtime::GameWorld;
using runtime::SystemPhase;

constexpr float kPlayerSpeed = 220.0F; // pixels per second
constexpr float kTileSize = 48.0F;
constexpr int kFloorColumns = 40;
constexpr int kFloorRows = 25;

constexpr std::string_view kRoomSceneUri = "res://scenes/smoke_room.scene.json";

// Adds one decorative sprite. Only the backdrop uses this: room content comes from
// the scene document.
auto SpawnBackdropTile(ecs::EntityWorld& entities, const float x, const float y, const std::int32_t sort_key) -> bool {
    const auto entity = entities.CreateEntity();
    if (!entities.Emplace<Transform2D>(entity, Transform2D{.position = {x, y}}).has_value())
        return false;
    if (!entities.Emplace<RenderTransform2D>(entity).has_value())
        return false;
    SpriteRenderer sprite{};
    sprite.texture_uri = "res://textures/floor.png";
    sprite.size = {kTileSize, kTileSize};
    sprite.pivot = {0.5F, 0.5F};
    sprite.layer = -10;
    sprite.sort_key = sort_key;
    return entities.Emplace<SpriteRenderer>(entity, sprite).has_value();
}

// The floor grid, until T5.1's Tilemap generates layers from a tileset. It is
// decoration: nothing in the scene document refers to it, and removing it would not
// change what the room *is*.
auto AddBackdrop(ecs::EntityWorld& entities) -> bool {
    for (auto row = 0; row < kFloorRows; ++row) {
        for (auto column = 0; column < kFloorColumns; ++column) {
            const auto x = kTileSize * (static_cast<float>(column) + 0.5F);
            const auto y = kTileSize * (static_cast<float>(row) + 0.5F);
            if (!SpawnBackdropTile(entities, x, y, row))
                return false;
        }
    }
    return true;
}

} // namespace

auto FindBySceneId(ecs::EntityWorld& entities, const std::uint32_t scene_id) -> ecs::Entity {
    for (const auto entity : entities.With<SceneEntityId>()) {
        const auto* id = entities.TryGet<SceneEntityId>(entity);
        if (id != nullptr && id->value == scene_id)
            return entity;
    }
    return {};
}

auto RegisterSmokeSystems(GameWorld& world) -> Result<void> {
    auto& scheduler = world.GetScheduler();

    // Movement. The player is resolved by scene id on every fixed step: a cached
    // handle would dangle the moment the room is reloaded.
    if (auto added = scheduler.Add(
            SystemPhase::FixedUpdate,
            runtime::SystemDescriptor{
                "move-player", [](GameWorld& world_ref, const FrameContext& context) -> Result<void> {
                    auto& entities = world_ref.GetEntities();
                    const auto player = FindBySceneId(entities, kPlayerSceneId);
                    if (!player.IsValid())
                        return Ok();

                    const auto& input = input::InputSystem::GetInstance();
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

                    const float length = std::sqrt(direction_x * direction_x + direction_y * direction_y);
                    direction_x /= length;
                    direction_y /= length;

                    auto* transform = entities.TryGet<Transform2D>(player);
                    if (transform == nullptr)
                        return Ok();
                    const float step = kPlayerSpeed * context.delta_seconds;
                    transform->position = {transform->position.GetX() + direction_x * step,
                                           transform->position.GetY() + direction_y * step};

                    // Facing is a sprite property, not a simulation one, so the flip
                    // lives next to the movement that causes it.
                    if (auto* sprite = entities.TryGet<SpriteRenderer>(player);
                        sprite != nullptr && direction_x != 0.0F) {
                        sprite->flip_x = direction_x < 0.0F;
                    }
                    return Ok();
                }});
        !added.has_value())
        return added;

    // Camera follow. Also resolves both entities by scene id, for the same reason.
    if (auto added = scheduler.Add(
            SystemPhase::LateUpdate,
            runtime::SystemDescriptor{
                "camera-follow", [](GameWorld& world_ref, const FrameContext& context) -> Result<void> {
                    auto& entities = world_ref.GetEntities();
                    const auto player = FindBySceneId(entities, kPlayerSceneId);
                    const auto camera = FindBySceneId(entities, kCameraSceneId);
                    if (!player.IsValid() || !camera.IsValid())
                        return Ok();

                    const auto* target = entities.TryGet<Transform2D>(player);
                    auto* eye = entities.TryGet<Transform2D>(camera);
                    if (target == nullptr || eye == nullptr)
                        return Ok();

                    // Frame-rate independent smoothing: the blend uses the variable
                    // delta the phase was given.
                    const float blend = 1.0F - std::exp(-8.0F * context.delta_seconds);
                    eye->position = algo::Vec2::Lerp(eye->position, target->position, blend);
                    return Ok();
                }});
        !added.has_value())
        return added;

    return Ok();
}

SmokeScreen::SmokeScreen()
    : GameScreen([](GameWorld& world) -> Result<void> { return RegisterSmokeSystems(world); }) {}

SmokeScreen::~SmokeScreen() {
    // The GPU device outlives the screen, so renderer-owned resources must be
    // released before the device goes away.
    renderer_.Shutdown();
}

auto SmokeScreen::LoadRoom() -> Result<void> {
    fs::AssetPath path{};
    if (!fs::AssetPath::TryParse(kRoomSceneUri, path)) {
        return Fail(MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "SmokeScreen::LoadRoom")
                        .WithDetail("the room URI is not a valid asset path: " + std::string{kRoomSceneUri}));
    }

    auto loaded = scene::SceneLoader::Load(fs::Vfs::GetInstance(), path);
    if (!loaded.has_value())
        return Fail(std::move(loaded.error()));

    // One call swaps the whole world. Everything above happened in a throwaway
    // storage, so a failure there left the running world exactly as it was.
    ReplaceWorldEntities(std::move(loaded->entities));

    // Decoration goes in after the swap: the swap replaces the storage, so anything
    // added to the old one would simply disappear.
    if (!AddBackdrop(World().GetEntities())) {
        return Fail(MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "SmokeScreen::LoadRoom")
                        .WithDetail("the backdrop floor could not be created"));
    }

    LOG_INFO(atom::log::core::Screen, "Loaded scene '" + loaded->description.id + "' from " +
                                          std::string{kRoomSceneUri} + " with " +
                                          std::to_string(loaded->description.entities.size()) + " entities");
    return Ok();
}

auto SmokeScreen::EntityCount() const -> std::size_t {
    return World().GetEntities().AliveCount();
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

    // Space toggles the world without stopping the host: the screen keeps rendering,
    // so a paused scene is visibly paused.
    SetPaused(!IsPaused());
    return true;
}

} // namespace atom::game

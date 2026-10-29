#include "TestGameScreen.hpp"

#include <cmath>
#include <iterator>
#include <span>
#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ECS/Components.hpp>
#include <Input/InputSystem.hpp>
#include <Log/LogSystem.hpp>
#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>
#include <Physics/Integration/Physics2DWorld.hpp>
#include <Runtime/System.hpp>
#include <Scene/SceneLoader.hpp>

#include "GameInput.hpp"

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

constexpr std::string_view kRoomSceneUri = "res://scenes/test_room.scene.json";

// Two event types are all this slice needs: the physics system publishes them, the goal system
// drains them. This is the shape the plan asks for - contacts become world events, so a system
// that reacts runs after the step instead of inside the solver's callback.
enum SliceEventType : std::uint32_t {
    kContactObserved = 1000,
    kExitTouched = 1001,
};

// The physics backend and world belong to the level load, and the systems are plain lambdas that
// cannot capture screen members (the base GameScreen constructor runs before any member exists).
// File-scope state is the honest place for both, and it mirrors Slice() below.
physics::InternalPhysicsBackend& PhysicsBackend() {
    static physics::InternalPhysicsBackend backend{};
    return backend;
}

std::unique_ptr<physics::integration::Physics2DWorld>& PhysicsWorld() {
    static std::unique_ptr<physics::integration::Physics2DWorld> world{};
    return world;
}

// --- backdrop --------------------------------------------------------------

auto SpawnBackdropTile(ecs::EntityWorld& entities, const float x, const float y, const std::int32_t sort_key)
    -> bool {
    const auto entity = entities.CreateEntity();
    if (!entities.Emplace<Transform2D>(entity, Transform2D{.position = {x, y}}).has_value())
        return false;
    // The display copy is created *with* the position: a default-constructed one is (0,0) until
    // the next fixed-step snapshot, and a frame can run zero steps, which would draw a whole
    // restart's worth of tiles at the world origin for that frame.
    if (!entities.Emplace<RenderTransform2D>(entity, RenderTransform2D{.position = {x, y}}).has_value())
        return false;
    SpriteRenderer sprite{};
    sprite.texture_uri = "res://textures/floor.png";
    sprite.size = {kTileSize, kTileSize};
    sprite.pivot = {0.5F, 0.5F};
    sprite.layer = -10;
    sprite.sort_key = sort_key;
    return entities.Emplace<SpriteRenderer>(entity, sprite).has_value();
}

// The floor grid, until T5.1's Tilemap generates layers from a tileset. Decoration: nothing in the
// scene document refers to it, and the room's *collision* comes from the wall entities, not this.
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

auto Slice() -> SliceState& {
    static SliceState state{};
    return state;
}

auto FindBySceneId(ecs::EntityWorld& entities, const std::uint32_t scene_id) -> ecs::Entity {
    for (const auto entity : entities.With<SceneEntityId>()) {
        const auto* id = entities.TryGet<SceneEntityId>(entity);
        if (id != nullptr && id->value == scene_id)
            return entity;
    }
    return {};
}

auto RegisterTestGameSystems(GameWorld& world) -> Result<void> {
    auto& scheduler = world.GetScheduler();

    // 1. Capture the display state first, every fixed step, so RenderTransform2D means "where
    //    things were one step ago" and interpolation has something to blend from.
    if (auto added = scheduler.Add(
            SystemPhase::FixedUpdate,
            runtime::SystemDescriptor{"capture-display", [](GameWorld& world_ref, const FrameContext&) -> Result<void> {
                                          render::scene2d::Scene2DRenderer::SnapshotTransforms(world_ref.GetEntities());
                                          return Ok();
                                      }});
        !added.has_value())
        return added;

    // 2. Movement drives the player's *velocity*, not its transform: the solver owns the
    //    position, which is what makes walls stop it and crates get pushed. Writing the velocity
    //    every step (including zero) is also the whole of "no input, no drift", because a
    //    top-down body has no friction to stop it.
    if (auto added = scheduler.Add(
            SystemPhase::FixedUpdate,
            runtime::SystemDescriptor{"move-player", [](GameWorld& world_ref, const FrameContext&) -> Result<void> {
                                          auto& physics = PhysicsWorld();
                                          if (physics == nullptr)
                                              return Ok();

                                          auto& entities = world_ref.GetEntities();
                                          const auto player = FindBySceneId(entities, kPlayerSceneId);
                                          if (!player.IsValid())
                                              return Ok();

                                          const auto& actions = GameInput::GetInstance().Actions();
                                          if (actions.IsTextInputCapturedByUi()) {
                                              physics->SetLinearVelocity(player, {0.0F, 0.0F});
                                              return Ok();
                                          }

                                          const auto direction =
                                              actions.Movement(GameAction::MoveLeft, GameAction::MoveRight,
                                                               GameAction::MoveUp, GameAction::MoveDown);
                                          physics->SetLinearVelocity(
                                              player, {direction.GetX() * kPlayerSpeed, direction.GetY() * kPlayerSpeed});

                                          // Facing is a sprite property, not a simulation one, so the flip
                                          // lives next to the movement that causes it.
                                          if (auto* sprite = entities.TryGet<SpriteRenderer>(player);
                                              sprite != nullptr && direction.GetX() != 0.0F) {
                                              sprite->flip_x = direction.GetX() < 0.0F;
                                          }
                                          return Ok();
                                      }});
        !added.has_value())
        return added;

    // 3. Step physics and publish contacts as events. Publishing instead of handling them here is
    //    the plan's "no callback re-entry" rule: the solver finishes, then gameplay reacts.
    if (auto added = scheduler.Add(
            SystemPhase::FixedUpdate,
            runtime::SystemDescriptor{"physics-step", [](GameWorld& world_ref, const FrameContext& context)
                                                           -> Result<void> {
                                          auto& physics = PhysicsWorld();
                                          if (physics == nullptr)
                                              return Ok();

                                          auto& entities = world_ref.GetEntities();
                                          auto& events = world_ref.GetEvents();
                                          auto& state = Slice();

                                          auto stepped = physics->Step(entities, context.delta_seconds);
                                          if (!stepped.has_value())
                                              return std::unexpected<Error>{std::move(stepped.error())};

                                          const auto exit = FindBySceneId(entities, kExitSceneId);
                                          const auto player = FindBySceneId(entities, kPlayerSceneId);

                                          for (const auto& contact : *stepped) {
                                              // Persist is reported every step for a resting pair, so
                                              // it maintains a live count instead of becoming an
                                              // event: enqueuing it would flood the queue (60/s per
                                              // pair) and make the HUD number meaningless.
                                              if (contact.phase == physics::integration::ContactPhase::Begin)
                                                  ++state.touching;
                                              else if (contact.phase == physics::integration::ContactPhase::End)
                                                  state.touching = state.touching > 0 ? state.touching - 1 : 0;
                                              else
                                                  continue;

                                              ++state.contact_events;
                                              events.Enqueue(
                                                  runtime::GameEvent{kContactObserved, contact.a, contact.b});

                                              // A sensor contact means "touched", not "blocked", so this is
                                              // where the exit becomes a goal instead of a wall.
                                              if (contact.phase != physics::integration::ContactPhase::Begin ||
                                                  !contact.sensor)
                                                  continue;
                                              const bool involves_exit = contact.a == exit || contact.b == exit;
                                              const bool involves_player = contact.a == player || contact.b == player;
                                              if (involves_exit && involves_player)
                                                  events.Enqueue(runtime::GameEvent{kExitTouched, player, exit});
                                          }
                                          return Ok();
                                      }});
        !added.has_value())
        return added;

    // 4. Drain the events in the gameplay phase. This system is why contacts became events: it can
    //    change game state (and, later, spawn or destroy entities through the command buffer)
    //    without touching the solver.
    if (auto added = scheduler.Add(
            SystemPhase::Update,
            runtime::SystemDescriptor{"goal", [](GameWorld& world_ref, const FrameContext&) -> Result<void> {
                                          auto drained = world_ref.GetEvents().Drain();
                                          auto& state = Slice();
                                          for (const auto& event : drained) {
                                              if (event.type == kExitTouched)
                                                  state.reached_exit = true;
                                          }
                                          return Ok();
                                      }});
        !added.has_value())
        return added;

    // 5. Camera follow on the variable-rate phase: the eye trails the player.
    if (auto added = scheduler.Add(
            SystemPhase::LateUpdate,
            runtime::SystemDescriptor{"camera-follow", [](GameWorld& world_ref, const FrameContext& context)
                                                            -> Result<void> {
                                          auto& entities = world_ref.GetEntities();
                                          const auto player = FindBySceneId(entities, kPlayerSceneId);
                                          const auto camera = FindBySceneId(entities, kCameraSceneId);
                                          if (!player.IsValid() || !camera.IsValid())
                                              return Ok();

                                          const auto* target = entities.TryGet<Transform2D>(player);
                                          auto* eye = entities.TryGet<Transform2D>(camera);
                                          if (target == nullptr || eye == nullptr)
                                              return Ok();

                                          const float blend = 1.0F - std::exp(-8.0F * context.delta_seconds);
                                          eye->position = algo::Vec2::Lerp(eye->position, target->position, blend);
                                          return Ok();
                                      }});
        !added.has_value())
        return added;

    // 6. The one-shot action: E toggles the camera lens between 1x and 2x.
    if (auto added = scheduler.Add(
            SystemPhase::Update,
            runtime::SystemDescriptor{"interact-zoom", [](GameWorld& world_ref, const FrameContext&) -> Result<void> {
                                          const auto& actions = GameInput::GetInstance().Actions();
                                          if (!actions.IsPressed(GameAction::Interact))
                                              return Ok();

                                          auto& entities = world_ref.GetEntities();
                                          const auto camera = FindBySceneId(entities, kCameraSceneId);
                                          if (!camera.IsValid())
                                              return Ok();
                                          auto* lens = entities.TryGet<ecs::Camera2D>(camera);
                                          if (lens == nullptr)
                                              return Ok();
                                          lens->zoom = lens->zoom > 1.5F ? 1.0F : 2.0F;
                                          return Ok();
                                      }});
        !added.has_value())
        return added;

    return Ok();
}

TestGameScreen::TestGameScreen()
    : GameScreen([](GameWorld& world) -> Result<void> { return RegisterTestGameSystems(world); }) {}

TestGameScreen::~TestGameScreen() {
    // The GPU device outlives the screen, so renderer-owned resources must be released first.
    renderer_.Shutdown();
    PhysicsWorld().reset();
}

auto TestGameScreen::LoadRoom() -> Result<void> {
    fs::AssetPath path{};
    if (!fs::AssetPath::TryParse(kRoomSceneUri, path)) {
        return Fail(MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "TestGameScreen::LoadRoom")
                        .WithDetail("the room URI is not a valid asset path: " + std::string{kRoomSceneUri}));
    }

    auto loaded = scene::SceneLoader::Load(fs::Vfs::GetInstance(), path);
    if (!loaded.has_value())
        return Fail(std::move(loaded.error()));

    // The physics world belongs to the level, not to the screen: rebuild it so a restart cannot
    // inherit bodies from the previous run.
    auto physics = physics::integration::Physics2DWorld::Create(PhysicsBackend(),
                                                               loaded->description.pixels_per_meter);
    if (physics == nullptr) {
        return Fail(MakeError(ErrorCode::PhysicsBackendCreationFailed, Module::Physics, "TestGameScreen::LoadRoom")
                        .WithDetail("the physics backend refused to create a world"));
    }
    physics->SetGravity(loaded->description.gravity);
    PhysicsWorld() = std::move(physics);

    // One call swaps the whole world. Everything above happened in a throwaway storage, so a
    // failure there left the running world exactly as it was.
    ReplaceWorldEntities(std::move(loaded->entities));

    if (!AddBackdrop(World().GetEntities())) {
        return Fail(MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "TestGameScreen::LoadRoom")
                        .WithDetail("the backdrop floor could not be created"));
    }

    Slice().Reset();

    LOG_INFO(atom::log::core::Screen, "Loaded scene '" + loaded->description.id + "' from " +
                                          std::string{kRoomSceneUri} + " with " +
                                          std::to_string(loaded->description.entities.size()) + " entities");
    return Ok();
}

auto TestGameScreen::EntityCount() const -> std::size_t {
    return World().GetEntities().AliveCount();
}

auto TestGameScreen::BodyCount() const -> std::size_t {
    const auto& world = PhysicsWorld();
    return world != nullptr ? world->BodyCount() : 0;
}

auto TestGameScreen::EnsureHudFont() -> render::Font* {
    if (hud_font_ != nullptr)
        return hud_font_;
    if (!hud_font_error_.empty())
        return nullptr;

    // A placeholder UI font. A game would ship its own; borrowing the one already in the tree keeps
    // the repository free of another binary asset for a demo.
    const std::string path = std::string{ATOM_SOURCE_ROOT} + "/ThirdParty/HarfBuzz/perf/fonts/Roboto-Regular.ttf";
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        hud_font_error_ = "HUD font not found: " + path;
        LOG_WARNING(atom::log::core::Screen, hud_font_error_);
        return nullptr;
    }
    // Read as chars, then reinterpret: istreambuf_iterator<char> into a byte vector does not
    // convert implicitly.
    const std::vector<char> raw{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    const std::span<const std::byte> bytes{reinterpret_cast<const std::byte*>(raw.data()), raw.size()};
    if (bytes.empty()) {
        hud_font_error_ = "HUD font is empty: " + path;
        return nullptr;
    }
    hud_font_ = renderer_.LoadFontFromMemory(bytes);
    if (hud_font_ == nullptr)
        hud_font_error_ = "HUD font could not be parsed: " + path;
    return hud_font_;
}

auto TestGameScreen::DrawHud() -> void {
    auto* font = EnsureHudFont();
    if (font == nullptr)
        return;

    const auto& state = Slice();
    const auto white = render::Color{235, 240, 250, 255};
    const auto dim = render::Color{150, 165, 190, 255};
    const auto accent = render::Color{120, 230, 150, 255};

    const std::string line = state.reached_exit ? "EXIT REACHED - press R to restart"
                                                : "WASD move   E zoom   Space pause   R restart";
    renderer_.DrawText(*font, line, 16.0F, 12.0F, state.reached_exit ? accent : white, 20.0F);

    const std::string stats = "entities " + std::to_string(EntityCount()) + "   bodies " +
                              std::to_string(BodyCount()) + "   touching " + std::to_string(state.touching) +
                              "   contact events " + std::to_string(state.contact_events) + "   zoom " +
                              (camera_zoom_ > 1.5F ? "2x" : "1x");
    renderer_.DrawText(*font, stats, 16.0F, 38.0F, dim, 16.0F);

    if (IsPaused()) {
        // A dim band plus a word: enough to read "the world is stopped" at a glance, without
        // building a real menu (that is a follow-up, not this slice's requirement).
        renderer_.DrawRect({0.0F, static_cast<float>(window_height_) * 0.5F - 28.0F,
                            static_cast<float>(window_width_), 56.0F},
                           render::Color{10, 12, 18, 190});
        const std::string paused = "PAUSED";
        const auto measured = renderer_.MeasureText(*font, paused, 32.0F);
        renderer_.DrawText(*font, paused, (static_cast<float>(window_width_) - measured.width) * 0.5F,
                           static_cast<float>(window_height_) * 0.5F - measured.height * 0.5F,
                           render::Color{255, 220, 140, 255}, 32.0F);
    }
}

auto TestGameScreen::Render(render::IRenderDevice& device) -> void {
    if (!renderer_.IsInitialized() && !renderer_.Initialize(device, ATOM_SHADER_OUTPUT_DIR)) {
        if (!render_failed_) {
            LOG_ERROR(atom::log::core::Screen, "TestGame: Renderer2D initialization failed");
            render_failed_ = true;
        }
        return;
    }

    const auto size = device.GetOutputSize();
    window_width_ = static_cast<std::uint32_t>(size.GetX());
    window_height_ = static_cast<std::uint32_t>(size.GetY());

    texture_cache_.PumpAsync();
    texture_cache_.FlushDeferredDestroys();

    auto& entities = World().GetEntities();
    const float alpha = InterpolationAlpha();
    const auto view = render::scene2d::Scene2DRenderer::FindActiveCamera(
        entities, {static_cast<float>(window_width_), static_cast<float>(window_height_)}, alpha);
    camera_zoom_ = view.zoom;

    scene_renderer_.Collect(entities, alpha);

    // Two passes inside one device frame: the world in camera space, then the HUD in screen space.
    // Submit() opens its own Renderer2D frame, which is why the HUD starts another one.
    scene_renderer_.Submit(renderer_, view);
    if (renderer_.BeginFrame(0.0F, 0.0F, 1.0F)) {
        DrawHud();
        renderer_.EndFrame();
    }
}

auto TestGameScreen::BeginFrame(const std::uint32_t fixed_steps, const float fixed_delta_seconds,
                             const float interpolation_alpha) -> void {
    GameScreen::BeginFrame(fixed_steps, fixed_delta_seconds, interpolation_alpha);

    // One place decides whether gameplay input is live, so pausing cannot be half-applied.
    GameInput::GetInstance().Actions().SetGameplayEnabled(!IsPaused());
}

auto TestGameScreen::HandleEvent(const window::IEvent& event) -> bool {

    if (event.type != window::EventType::KeyPressed)
        return false;

    // The event path asks the *bindings* which action this key means, which is why the pause and
    // restart keys are rebindable and why they still work while the gameplay gate is closed.
    const auto& key = std::get<window::KeyEvent>(event.data);
    const auto action = GameInput::GetInstance().Actions().FindAction(key);
    if (!action.has_value())
        return false;

    if (*action == GameAction::Pause) {
        SetPaused(!IsPaused());
        return true;
    }
    if (*action == GameAction::Restart) {
        // Reload from the document, not from a snapshot: restart must exercise the same load path
        // as boot, otherwise the two can drift apart.
        if (auto reloaded = LoadRoom(); !reloaded.has_value())
            LOG_ERROR(atom::log::core::Screen, "Restart failed: " + reloaded.error().Message());
        SetPaused(false);
        return true;
    }
    return false;
}

} // namespace atom::game

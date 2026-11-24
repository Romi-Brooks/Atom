// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <World2D/Animation2D.hpp>
#include <World2D/CameraSystem2D.hpp>
#include <World2D/ObjectLayer2D.hpp>
#include <World2D/ParticleSystem2D.hpp>
#include <World2D/TilemapLoader.hpp>
#include <World2D/TilemapPhysics2D.hpp>
#include <World2D/World2DRenderer.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>

namespace {
class StubResolver final : public atom::render::scene2d::ITextureResolver {
    public:
        auto Resolve(std::string_view uri) -> atom::render::Renderer2D::Texture* override {
            return &textures_[std::string{uri}];
        }

    private:
        std::unordered_map<std::string, atom::render::Renderer2D::Texture> textures_{};
};
} // namespace

using namespace atom;

auto main() -> int {
    fs::AssetPath fixture_path{};
    ATOM_CHECK(fs::AssetPath::TryParse("res://sample.tilemap.json", fixture_path));
    std::unique_ptr<fs::NativeFileSystem> fixture_fs{};
    ATOM_CHECK(fs::NativeFileSystem::Create("res", std::string{ATOM_SOURCE_ROOT} + "/Test/World2D/Fixtures",
                                            fixture_fs) == fs::Result::Success);
    auto disk_map = world2d::LoadTilemap(*fixture_fs, fixture_path);
    ATOM_CHECK(disk_map.has_value());
    ATOM_CHECK(disk_map->Id() == "sample");

    constexpr auto document = R"({
      "schema_version":1,"kind":"tilemap","id":"test-map","projection":"orthogonal",
      "tile_size":[16,16],"chunk_size":2,
      "tiles":[{"id":1,"texture":"res://tiles.png","source":[0,0,16,16],"solid":true}],
      "layers":[
        {"kind":"tiles","name":"ground","collision":true,"chunks":[
          {"x":0,"y":0,"cells":[1,1,0,0]}]},
        {"kind":"objects","name":"actors","objects":[
          {"id":7,"prefab":"res://actor.prefab.json","position":[5,6]}]}
      ]})";
    auto parsed = world2d::ParseTilemap(document, "res://map.tilemap.json");
    ATOM_CHECK(parsed.has_value());
    auto& map = *parsed;
    ATOM_CHECK(map.Id() == "test-map");
    ATOM_CHECK(map.GetCell(0, 0, 0) == 1);
    ATOM_CHECK(map.GetCell(0, 1, 1) == 0);
    ATOM_CHECK(map.ObjectLayers().at(0).objects.at(0).stable_id == 7);
    ATOM_CHECK(map.ObjectLayers().at(0).objects.at(0).prefab_uri == "res://actor.prefab.json");
    auto colliders = map.BuildColliders();
    ATOM_CHECK(colliders.size() == 1);
    ATOM_CHECK_NEAR(colliders[0].bounds.width, 32.0f);
    ATOM_CHECK_NEAR(colliders[0].bounds.height, 16.0f);

    // Negative cells retain floor-division chunk identity and can unload alone.
    ATOM_CHECK(map.SetCell(0, -1, 0, 1));
    ATOM_CHECK(map.GetCell(0, -1, 0) == 1);
    colliders = map.BuildColliders();
    ATOM_CHECK(colliders.size() == 2);
    std::size_t visible = 0;
    map.ForEachCell(0, algo::Rect{0, 0, 16, 16}, [&](auto, auto, const world2d::TileDefinition&) { ++visible; });
    ATOM_CHECK(visible == 1);
    ATOM_CHECK(
        !world2d::ParseTilemap(std::string{document}.replace(std::string{document}.find("[1,1,0,0]"), 9, "[1,9,0,0]"),
                               "res://invalid.tilemap.json")
             .has_value());

    auto created_world = ecs::EntityWorld::Create();
    ATOM_CHECK(created_world.has_value());
    auto& world = **created_world;
    auto bodies = world2d::CreateTileColliders(map, world, 16.0f);
    ATOM_CHECK(bodies.has_value());
    ATOM_CHECK(bodies->entities.size() == 2);
    ATOM_CHECK(world.AliveCount() == 2);
    const auto* first_body = world.TryGet<ecs::PhysicsBody2D>(bodies->entities.at(0).entity);
    ATOM_CHECK(first_body != nullptr);
    ATOM_CHECK(first_body->kind == ecs::PhysicsBodyKind::Static);
    ATOM_CHECK_NEAR(first_body->half_extent.GetY(), 0.5f);
    world2d::DestroyChunkColliders(world, *bodies, 0, world2d::ChunkCoord{-1, 0});
    ATOM_CHECK(bodies->entities.size() == 1);
    ATOM_CHECK(world.AliveCount() == 1);
    world2d::DestroyTileColliders(world, *bodies);
    ATOM_CHECK(world.AliveCount() == 0);
    ATOM_CHECK(map.RemoveChunk(0, {-1, 0}));
    ATOM_CHECK(map.GetCell(0, -1, 0) == 0);
    ATOM_CHECK(!map.RemoveChunk(0, {-1, 0}));
    auto objects = world2d::InstantiateObjects(
        map, world, [](ecs::EntityWorld& target, const world2d::MapObject&) -> Result<ecs::Entity> {
            return target.CreateEntity();
        });
    ATOM_CHECK(objects.has_value());
    ATOM_CHECK(objects->size() == 1);
    ATOM_CHECK(objects->at(0).stable_id == 7);
    ATOM_CHECK_NEAR(world.TryGet<ecs::Transform2D>(objects->at(0).entity)->position.GetX(), 5.0f);
    world2d::DestroyObjects(world, *objects);
    ATOM_CHECK(world.AliveCount() == 0);

    world2d::AnimationLibrary2D clips{};
    world2d::AnimationClip2D clip{};
    clip.name = "walk";
    clip.texture_uri = "res://walk.png";
    clip.frames = {{algo::Rect{0, 0, 16, 16}, 0.1f, 0}, {algo::Rect{16, 0, 16, 16}, 0.1f, 42}};
    ATOM_CHECK(clips.AddClip(std::move(clip)));
    ATOM_CHECK(clips.BindState("moving", "walk"));
    ATOM_CHECK(clips.BindState("idle", "walk"));
    ATOM_CHECK(clips.AddTransition("moving", "stop", "idle"));
    ATOM_CHECK(!clips.AddTransition("moving", "stop", "idle"));
    world2d::AnimationSystem2D animation{clips};
    const auto actor = world.CreateEntity();
    ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(actor, ecs::SpriteRenderer{}).has_value());
    ATOM_CHECK(world.Emplace<ecs::Animator2D>(actor, ecs::Animator2D{}).has_value());
    ATOM_CHECK(animation.SetState(world, actor, "moving"));
    ATOM_CHECK(animation.Step(world, 0.11f).size() == 1);
    ATOM_CHECK(animation.Step(world, 0.11f).empty());
    ATOM_CHECK_NEAR(world.TryGet<ecs::SpriteRenderer>(actor)->source_position.GetX(), 0.0f);
    world.TryGet<ecs::Animator2D>(actor)->loop = false;
    ATOM_CHECK(animation.Step(world, 0.25f).size() == 1);
    ATOM_CHECK(world.TryGet<ecs::Animator2D>(actor)->finished);
    ATOM_CHECK(animation.Trigger(world, actor, "stop"));
    ATOM_CHECK(world.TryGet<ecs::Animator2D>(actor)->state == "idle");
    ATOM_CHECK(!animation.Trigger(world, actor, "stop"));

    const auto target = world.CreateEntity();
    ATOM_CHECK(world.Emplace<ecs::Transform2D>(target, ecs::Transform2D{.position = {90, 90}}).has_value());
    ATOM_CHECK(world.Emplace<ecs::SceneEntityId>(target, ecs::SceneEntityId{7}).has_value());
    const auto camera = world.CreateEntity();
    ATOM_CHECK(world.Emplace<ecs::Transform2D>(camera, ecs::Transform2D{}).has_value());
    ATOM_CHECK(world.Emplace<ecs::Camera2D>(camera, ecs::Camera2D{}).has_value());
    ecs::CameraRig2D rig{};
    rig.target_scene_id = 7;
    rig.follow_half_life = 0;
    rig.clamp_to_bounds = true;
    rig.bounds_max = {100, 100};
    ATOM_CHECK(world.Emplace<ecs::CameraRig2D>(camera, rig).has_value());
    ATOM_CHECK(world2d::CameraSystem2D::Step(world, {40, 40}, 0.016f) == 1);
    ATOM_CHECK_NEAR(world.TryGet<ecs::Transform2D>(camera)->position.GetX(), 80.0f);
    ATOM_CHECK(world2d::CameraSystem2D::Shake(world, camera, 2.0f, 0.1f));
    ATOM_CHECK(world2d::CameraSystem2D::Step(world, {40, 40}, 0.1f) == 1);
    ATOM_CHECK_NEAR(world.TryGet<ecs::Transform2D>(camera)->position.GetX(), 80.0f);
    ATOM_CHECK(world2d::CameraSystem2D::PanTo(world, camera, {60, 60}, 1.0f));
    ATOM_CHECK(world2d::CameraSystem2D::Step(world, {40, 40}, 0.5f) == 1);
    ATOM_CHECK_NEAR(world.TryGet<ecs::Transform2D>(camera)->position.GetX(), 70.0f);
    ATOM_CHECK(world2d::CameraSystem2D::Step(world, {40, 40}, 0.5f) == 1);
    ATOM_CHECK_NEAR(world.TryGet<ecs::Transform2D>(camera)->position.GetX(), 60.0f);

    world2d::ParticleSystem2D particles{5, 1};
    world2d::ParticlePreset2D preset{};
    preset.name = "spark";
    preset.texture_uri = "res://spark.png";
    preset.source = {0, 0, 8, 8};
    preset.spawn_rate = 10;
    preset.lifetime_min = preset.lifetime_max = 1;
    preset.speed_min = preset.speed_max = 0;
    preset.layer = 2;
    ATOM_CHECK(particles.AddPreset(preset));
    const auto emitter = world.CreateEntity();
    ATOM_CHECK(world.Emplace<ecs::Transform2D>(emitter, ecs::Transform2D{}).has_value());
    ATOM_CHECK(world.Emplace<ecs::ParticleEmitter2D>(emitter, ecs::ParticleEmitter2D{.preset = "spark"}).has_value());
    const auto first_spawn = particles.Step(world, 1, 0.5f);
    ATOM_CHECK_MSG(first_spawn == 5, std::string{"emitted "} + std::to_string(first_spawn));
    ATOM_CHECK(particles.Count() == 5);
    ATOM_CHECK(particles.Collect().size() == 5);
    ATOM_CHECK(particles.Collect().front().filter == ecs::SpriteFilter::Linear);
    ATOM_CHECK(particles.Step(world, 1, 0.5f) == 0);
    ATOM_CHECK(particles.Step(world, 1, 0.5f) == 5);
    ATOM_CHECK(particles.Step(world, 2, 0.0f) == 0);
    ATOM_CHECK(particles.Count() == 0);
    ATOM_CHECK(particles.Burst("spark", {0, 0}, 1) == 1);
    particles.Advance(0.5f);
    ATOM_CHECK(particles.Count() == 1);
    particles.Advance(0.5f);
    ATOM_CHECK(particles.Count() == 0);
    preset.name = "pixel-spark";
    preset.filter = ecs::SpriteFilter::Nearest;
    ATOM_CHECK(particles.AddPreset(preset));
    ATOM_CHECK(particles.Burst("pixel-spark", {0, 0}, 20) == 5);
    auto* sprite = world.TryGet<ecs::SpriteRenderer>(actor);
    sprite->layer = 1;
    sprite->size = {16, 16};
    ATOM_CHECK(world.Emplace<ecs::Transform2D>(actor, ecs::Transform2D{}).has_value());
    ATOM_CHECK(world.Emplace<ecs::RenderTransform2D>(actor, ecs::RenderTransform2D{}).has_value());
    StubResolver resolver{};
    world2d::World2DRenderer renderer{resolver};
    const render::scene2d::CameraView view{{8, 8}, {32, 32}, 1};
    const auto& draws = renderer.Collect(world, view, &map, &particles);
    ATOM_CHECK(draws.size() == 8); // 2 visible tiles, 1 sprite, 5 particles
    ATOM_CHECK(draws.front().kind == 0);
    ATOM_CHECK(draws.at(2).kind == 1);
    ATOM_CHECK(draws.back().kind == 2);
    ATOM_CHECK(draws.back().filter == render::Filter2D::Nearest);
    // Partially visible draws stay and offscreen draws are culled only when requested.
    ATOM_CHECK(renderer.Collect(world, view, &map, &particles, 1.0f, true).size() == 8);
    const render::scene2d::CameraView distant_view{{256, 256}, {32, 32}, 2};
    ATOM_CHECK(renderer.Collect(world, distant_view, nullptr, &particles).size() == 6);
    ATOM_CHECK(renderer.Collect(world, distant_view, nullptr, &particles, 1.0f, true).empty());

    particles.Clear();
    preset.name = "low-spark";
    preset.layer = -1;
    ATOM_CHECK(particles.AddPreset(preset));
    ATOM_CHECK(particles.Burst("pixel-spark", {0, 0}, 1) == 1);
    ATOM_CHECK(particles.Burst("low-spark", {0, 0}, 1) == 1);
    ATOM_CHECK(particles.Collect().front().layer == -1);
    ATOM_CHECK(particles.Collect(false).front().layer == 2);
    const auto& ordered = renderer.Collect(world, view, nullptr, &particles, 1.0f, true);
    ATOM_CHECK(ordered.size() == 3);
    ATOM_CHECK(ordered.front().layer == -1 && ordered.back().layer == 2);
    return 0;
}

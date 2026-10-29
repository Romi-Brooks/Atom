// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Scene/SceneLoader.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ECS/Components.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Log/LogSystem.hpp>
#include <Scene/SceneSchema.hpp>

namespace atom::scene {

namespace {

using ecs::Camera2D;
using ecs::Entity;
using ecs::EntityWorld;
using ecs::Lifetime;
using ecs::Name;
using ecs::RenderTransform2D;
using ecs::SceneEntityId;
using ecs::SpriteRenderer;
using ecs::Transform2D;

// Reads and caches prefab documents for the duration of one scene load.
//
// Per-load rather than global on purpose: a prefab edited between two loads must be
// seen by the second one, and a long-lived cache would make "why is my prefab change
// ignored" the next mystery.
class PrefabCache final {
    public:
        explicit PrefabCache(const fs::IFileSystem& filesystem) : filesystem_(&filesystem) {}

        [[nodiscard]] auto Get(const std::string_view uri) -> Result<EntityDesc> {
            const std::string key{uri};
            if (const auto found = cache_.find(key); found != cache_.end())
                return found->second;

            fs::AssetPath path{};
            if (!fs::AssetPath::TryParse(uri, path))
                return PrefabError(uri, "the prefab URI is not a valid asset path");

            std::unique_ptr<fs::IFile> file{};
            if (filesystem_->OpenRead(path, file) != fs::Result::Success || !file)
                return PrefabError(uri, "the prefab file could not be opened");

            std::vector<std::byte> bytes{};
            if (fs::ReadAll(*file, bytes) != fs::Result::Success)
                return PrefabError(uri, "the prefab file could not be read");

            const std::string_view text{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
            auto outcome = ParsePrefab(text, path.String());
            if (!outcome.Ok()) {
                for (const auto& diagnostic : outcome.diagnostics)
                    LOG_WARNING(atom::log::core::Main, "prefab load: " + diagnostic.Format());
                std::string detail = outcome.diagnostics.front().Format();
                if (outcome.diagnostics.size() > 1)
                    detail += " (+" + std::to_string(outcome.diagnostics.size() - 1) + " more)";
                return PrefabError(uri, std::move(detail));
            }

            auto stored = cache_.emplace(key, std::move(*outcome.entity));
            return stored.first->second;
        }

    private:
        [[nodiscard]] static auto PrefabError(const std::string_view uri, const std::string& detail) -> Result<EntityDesc> {
            return std::unexpected<Error>{MakeError(ErrorCode::SceneInvalidField, Module::Scene, "SceneLoader::Prefab")
                                              .WithResource(std::string{uri})
                                              .WithDetail(detail)};
        }

        const fs::IFileSystem* filesystem_ = nullptr;
        std::unordered_map<std::string, EntityDesc> cache_{};
};

auto BuildFailure(const SceneDesc& scene, const EntityDesc& entity, const std::string& what) -> Error {
    std::string detail = "entity " + std::to_string(entity.id);
    if (entity.has_name)
        detail += " (\"" + entity.name + "\")";
    detail += ": ";
    detail += what;
    return MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "SceneLoader::BuildWorld")
        .WithResource("scene://" + scene.id)
        .WithDetail(std::move(detail));
}

// Adds one entity's components. Returns false on the first ECS-level refusal, which
// can only mean the description was built by something other than ParseScene.
auto BuildEntity(EntityWorld& world, const EntityDesc& entity) -> Result<void> {
    const auto handle = world.CreateEntity();

    if (auto placed = world.Emplace<SceneEntityId>(handle, SceneEntityId{.value = entity.id}); !placed.has_value())
        return placed;
    if (auto placed = world.Emplace<Transform2D>(handle,
                                                 Transform2D{.position = entity.transform.position,
                                                             .rotation = entity.transform.rotation,
                                                             .scale = entity.transform.scale});
        !placed.has_value())
        return placed;
    // The display copy starts equal to the simulation state: rendering before the
    // first snapshot then draws the authored position, not the origin.
    if (auto placed = world.Emplace<RenderTransform2D>(handle,
                                                      RenderTransform2D{.position = entity.transform.position,
                                                                        .rotation = entity.transform.rotation,
                                                                        .scale = entity.transform.scale});
        !placed.has_value())
        return placed;

    if (entity.has_name) {
        if (auto placed = world.Emplace<Name>(handle, Name{.value = entity.name}); !placed.has_value())
            return placed;
    }
    if (entity.active) {
        if (auto placed = world.Emplace<ecs::Active>(handle); !placed.has_value())
            return placed;
    }
    if (entity.is_static) {
        if (auto placed = world.Emplace<ecs::Static>(handle); !placed.has_value())
            return placed;
    }

    if (entity.sprite.has_value()) {
        const auto& desc = *entity.sprite;
        SpriteRenderer sprite{};
        sprite.texture_uri = desc.texture_uri;
        if (desc.has_source) {
            sprite.source_position = {desc.source.x, desc.source.y};
            sprite.source_size = {desc.source.width, desc.source.height};
        }
        if (desc.has_size)
            sprite.size = desc.size;
        sprite.pivot = desc.pivot;
        sprite.tint = desc.tint;
        sprite.layer = desc.layer;
        sprite.sort_key = desc.sort_key;
        sprite.flip_x = desc.flip_x;
        sprite.flip_y = desc.flip_y;
        if (auto placed = world.Emplace<SpriteRenderer>(handle, sprite); !placed.has_value())
            return placed;
    }

    if (entity.camera.has_value()) {
        Camera2D camera{};
        camera.zoom = entity.camera->zoom;
        camera.viewport_size = entity.camera->viewport;
        if (auto placed = world.Emplace<Camera2D>(handle, camera); !placed.has_value())
            return placed;
    }

    if (entity.lifetime.has_value()) {
        if (auto placed = world.Emplace<Lifetime>(handle, Lifetime{.remaining_seconds = *entity.lifetime});
            !placed.has_value())
            return placed;
    }

    return Ok();
}

} // namespace

auto SceneLoader::BuildWorld(const SceneDesc& description) -> Result<std::unique_ptr<EntityWorld>> {
    auto created = EntityWorld::Create();
    if (!created.has_value()) {
        return std::unexpected<Error>{
            MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "SceneLoader::BuildWorld")
                .WithResource("scene://" + description.id)
                .WithDetail("EntityWorld::Create failed: " + created.error().Message())};
    }
    auto world = std::move(*created);

    // The active camera goes in first so it ends up with the lowest entity index;
    // Render/Scene2D picks the lowest-index camera and therefore needs no "active"
    // flag of its own.
    if (description.active_camera < description.entities.size()) {
        const auto& camera_entity = description.entities[description.active_camera];
        if (auto built = BuildEntity(*world, camera_entity); !built.has_value()) {
            return std::unexpected<Error>{BuildFailure(description, camera_entity, built.error().Message())};
        }
    }

    for (std::size_t index = 0; index < description.entities.size(); ++index) {
        if (index == description.active_camera)
            continue;
        const auto& entity = description.entities[index];
        if (auto built = BuildEntity(*world, entity); !built.has_value())
            return std::unexpected<Error>{BuildFailure(description, entity, built.error().Message())};
    }

    return world;
}

auto SceneLoader::Load(const fs::IFileSystem& filesystem, const fs::AssetPath& path) -> Result<LoadedScene> {
    const auto read_failure = [&path](const std::string_view what) -> Error {
        return MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "SceneLoader::Load")
            .WithResource(path.String())
            .WithDetail(std::string{what});
    };

    std::unique_ptr<fs::IFile> file{};
    if (filesystem.OpenRead(path, file) != fs::Result::Success || !file)
        return std::unexpected<Error>{read_failure("the scene file could not be opened")};

    std::vector<std::byte> bytes{};
    if (fs::ReadAll(*file, bytes) != fs::Result::Success)
        return std::unexpected<Error>{read_failure("the scene file could not be read")};

    const std::string_view text{reinterpret_cast<const char*>(bytes.data()), bytes.size()};

    // Prefabs are resolved during validation, not after it: an instance's base body
    // has to exist before its overrides can be checked against it, and a prefab that
    // fails to load must fail the scene load rather than produce a hole.
    PrefabCache prefabs{filesystem};
    const PrefabProvider provider = [&prefabs](const std::string_view uri) { return prefabs.Get(uri); };

    auto outcome = ParseScene(text, path.String(), provider);
    if (!outcome.scene.has_value()) {
        // Every diagnostic is logged, so a five-error file does not hide four of them;
        // the returned Error carries the first, which is the one a UI shows.
        for (const auto& diagnostic : outcome.diagnostics)
            LOG_WARNING(atom::log::core::Main, "scene load: " + diagnostic.Format());

        std::string detail = outcome.diagnostics.front().Format();
        if (outcome.diagnostics.size() > 1)
            detail += " (+" + std::to_string(outcome.diagnostics.size() - 1) + " more)";
        return std::unexpected<Error>{MakeError(ErrorCode::SceneInvalidField, Module::Scene, "SceneLoader::Load")
                                          .WithResource(path.String())
                                          .WithDetail(std::move(detail))};
    }

    LoadedScene loaded{};
    loaded.description = std::move(*outcome.scene);
    auto world = BuildWorld(loaded.description);
    if (!world.has_value())
        return std::unexpected<Error>{std::move(world.error()).WithResource(path.String())};
    loaded.entities = std::move(*world);
    return loaded;
}

} // namespace atom::scene

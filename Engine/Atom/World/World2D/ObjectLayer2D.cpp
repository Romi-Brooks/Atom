// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Atom/World/World2D/ObjectLayer2D.hpp>

#include <utility>

#include <Atom/World/ECS/Components.hpp>

namespace atom::world2d {

auto DestroyObjects(ecs::EntityWorld& world, const std::vector<MapObjectEntity>& handles) -> void {
    for (const auto& handle : handles) {
        if (world.IsAlive(handle.entity))
            static_cast<void>(world.Destroy(handle.entity));
    }
}

auto InstantiateObjects(const Tilemap& map, ecs::EntityWorld& world,
                        const MapObjectFactory& factory) -> Result<std::vector<MapObjectEntity>> {
    if (!map.IsValid() || !factory)
        return std::unexpected<Error>{MakeError(ErrorCode::SceneInvalidField, Module::Scene,
                                                "InstantiateObjects").WithDetail("invalid map or object factory")};
    std::vector<MapObjectEntity> created{};
    for (const auto& layer : map.ObjectLayers()) {
        for (const auto& object : layer.objects) {
            auto entity = factory(world, object);
            if (!entity) {
                DestroyObjects(world, created);
                return std::unexpected<Error>{std::move(entity.error())};
            }
            if (!world.IsAlive(*entity)) {
                DestroyObjects(world, created);
                return std::unexpected<Error>{MakeError(ErrorCode::SceneInvalidField, Module::Scene,
                                                        "InstantiateObjects")
                                                  .WithDetail("factory returned a dead entity")};
            }
            created.push_back(MapObjectEntity{object.stable_id, *entity});
            auto* transform = world.TryGet<ecs::Transform2D>(*entity);
            if (transform == nullptr) {
                auto placed = world.Emplace<ecs::Transform2D>(*entity, ecs::Transform2D{});
                if (!placed) {
                    DestroyObjects(world, created);
                    return std::unexpected<Error>{std::move(placed.error())};
                }
                transform = world.TryGet<ecs::Transform2D>(*entity);
            }
            transform->position = object.position;
            transform->rotation = object.rotation;
            if (auto* render = world.TryGet<ecs::RenderTransform2D>(*entity); render != nullptr) {
                render->position = object.position;
                render->rotation = object.rotation;
            }
        }
    }
    return created;
}

} // namespace atom::world2d

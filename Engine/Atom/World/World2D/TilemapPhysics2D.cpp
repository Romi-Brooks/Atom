// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Atom/World/World2D/TilemapPhysics2D.hpp>

#include <cmath>
#include <algorithm>

#include <Atom/World/ECS/Components.hpp>

namespace atom::world2d {

auto DestroyTileColliders(ecs::EntityWorld& world, const TileColliderEntities& handles) -> void {
    for (const auto& handle : handles.entities) {
        if (world.IsAlive(handle.entity))
            static_cast<void>(world.Destroy(handle.entity));
    }
}

auto DestroyChunkColliders(ecs::EntityWorld& world, TileColliderEntities& handles,
                           const std::size_t layer_index, const ChunkCoord chunk) -> void {
    std::erase_if(handles.entities, [&](const TileColliderEntity& handle) {
        if (handle.layer_index != layer_index || handle.chunk != chunk)
            return false;
        if (world.IsAlive(handle.entity))
            static_cast<void>(world.Destroy(handle.entity));
        return true;
    });
}

auto CreateTileColliders(const Tilemap& map, ecs::EntityWorld& world,
                         const float pixels_per_meter) -> Result<TileColliderEntities> {
    if (!map.IsValid() || !std::isfinite(pixels_per_meter) || pixels_per_meter <= 0.0f)
        return std::unexpected<Error>{MakeError(ErrorCode::PhysicsInvalidShape, Module::Physics,
                                                "CreateTileColliders").WithDetail("invalid tilemap or pixel scale")};
    TileColliderEntities created{};
    for (const auto& collider : map.BuildColliders()) {
        const auto entity = world.CreateEntity();
        if (!entity.IsValid()) {
            DestroyTileColliders(world, created);
            return std::unexpected<Error>{MakeError(ErrorCode::EcsOutOfEntities, Module::Ecs,
                                                    "CreateTileColliders")};
        }
        created.entities.push_back(TileColliderEntity{entity, collider.layer_index, collider.chunk});
        const auto center = algo::Vec2{collider.bounds.x + collider.bounds.width * 0.5f,
                                       collider.bounds.y + collider.bounds.height * 0.5f};
        const auto half = algo::Vec2{collider.bounds.width * 0.5f / pixels_per_meter,
                                     collider.bounds.height * 0.5f / pixels_per_meter};
        const auto transform = ecs::Transform2D{.position = center};
        const auto body = ecs::PhysicsBody2D{.kind = ecs::PhysicsBodyKind::Static,
                                             .radius = 0.0f,
                                             .half_extent = half};
        auto placed = world.Emplace<ecs::Transform2D>(entity, transform);
        if (!placed.has_value()) {
            DestroyTileColliders(world, created);
            return std::unexpected<Error>{std::move(placed.error())};
        }
        placed = world.Emplace<ecs::PhysicsBody2D>(entity, body);
        if (!placed.has_value()) {
            DestroyTileColliders(world, created);
            return std::unexpected<Error>{std::move(placed.error())};
        }
    }
    return created;
}

} // namespace atom::world2d

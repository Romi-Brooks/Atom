// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file TilemapPhysics2D.hpp
 * @brief Converts merged tile colliders to ECS static physics shells.
 */
#ifndef ATOM_WORLD2D_TILEMAPPHYSICS2D_HPP
#define ATOM_WORLD2D_TILEMAPPHYSICS2D_HPP

#include <vector>

#include <Atom/World/ECS/EntityWorld.hpp>
#include <Atom/World/World2D/Tilemap.hpp>

namespace atom::world2d {

struct TileColliderEntity final {
        ecs::Entity entity{};
        std::size_t layer_index = 0;
        ChunkCoord chunk{};
};

struct TileColliderEntities final {
        std::vector<TileColliderEntity> entities{};
};

// Invoke after loading chunks and before Physics2DWorld::Step. The returned
// handles let a WorldSession remove colliders when it unloads those chunks.
[[nodiscard]] auto CreateTileColliders(const Tilemap& map, ecs::EntityWorld& world,
                                       float pixels_per_meter) -> Result<TileColliderEntities>;
auto DestroyTileColliders(ecs::EntityWorld& world, const TileColliderEntities& handles) -> void;
auto DestroyChunkColliders(ecs::EntityWorld& world, TileColliderEntities& handles,
                           std::size_t layer_index, ChunkCoord chunk) -> void;

} // namespace atom::world2d

#endif // ATOM_WORLD2D_TILEMAPPHYSICS2D_HPP

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#ifndef ATOM_WORLD2D_OBJECTLAYER2D_HPP
#define ATOM_WORLD2D_OBJECTLAYER2D_HPP

#include <functional>
#include <vector>

#include <ECS/EntityWorld.hpp>
#include <World2D/Tilemap.hpp>

namespace atom::world2d {

// The game supplies prefab construction. The engine applies placement and
// collects handles so a map or region can be unloaded. One callback invocation
// must return one live root entity; extra entities remain the game's concern.
using MapObjectFactory = std::function<Result<ecs::Entity>(ecs::EntityWorld&, const MapObject&)>;

struct MapObjectEntity final {
        std::uint64_t stable_id = 0;
        ecs::Entity entity{};
};

[[nodiscard]] auto InstantiateObjects(const Tilemap& map, ecs::EntityWorld& world,
                                      const MapObjectFactory& factory) -> Result<std::vector<MapObjectEntity>>;
auto DestroyObjects(ecs::EntityWorld& world, const std::vector<MapObjectEntity>& handles) -> void;

} // namespace atom::world2d

#endif // ATOM_WORLD2D_OBJECTLAYER2D_HPP

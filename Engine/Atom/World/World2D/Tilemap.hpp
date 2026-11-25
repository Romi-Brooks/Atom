// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Tilemap.hpp
 * @brief Sparse, chunk-addressed orthogonal tile and object layers.
 */
#ifndef ATOM_WORLD2D_TILEMAP_HPP
#define ATOM_WORLD2D_TILEMAP_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Atom/Core/Algorithm/Geometry/Rect.hpp>
#include <Atom/Core/Algorithm/Vector/Vec2.hpp>
#include <Atom/World/ECS/Components.hpp>

namespace atom::world2d {

// Projection is explicit in the data contract. Other projections must supply
// their own cell-to-world and collision policy; they cannot silently use the
// orthogonal implementation below.
enum class TileProjection : std::uint8_t { Orthogonal };

struct TileDefinition final {
        std::uint32_t id = 0; // zero is always empty
        std::string texture_uri{};
        algo::Rect source{}; // texels; positive size is required
        ecs::SpriteFilter filter = ecs::SpriteFilter::Nearest;
        bool solid = false;
};

struct ChunkCoord final {
        std::int32_t x = 0;
        std::int32_t y = 0;
        [[nodiscard]] auto operator==(const ChunkCoord&) const noexcept -> bool = default;
};

struct ChunkCoordHash final {
        [[nodiscard]] auto operator()(ChunkCoord coord) const noexcept -> std::size_t;
};

struct TileChunk final {
        ChunkCoord coord{};
        // Row-major, exactly chunk_size * chunk_size entries. Empty cells are 0.
        std::vector<std::uint32_t> cells{};
};

struct TileLayer final {
        std::string name{};
        std::int32_t draw_order = 0;
        bool visible = true;
        bool collision_enabled = false;
        algo::Vec2 parallax{1.0f, 1.0f};
        std::unordered_map<ChunkCoord, TileChunk, ChunkCoordHash> chunks{};
};

// The map owns placement data, while the game decides what a prefab means. This
// keeps tile/object authoring reusable for RPG, platformer and shooter rules.
struct MapObject final {
        std::uint64_t stable_id = 0;
        std::string prefab_uri{};
        algo::Vec2 position{};
        float rotation = 0.0f;
};

struct ObjectLayer final {
        std::string name{};
        std::vector<MapObject> objects{};
};

struct TileCollider final {
        std::size_t layer_index = 0;
        ChunkCoord chunk{};
        algo::Rect bounds{}; // world pixels
};

class Tilemap final {
    public:
        Tilemap(algo::Vec2 tile_size, std::uint32_t chunk_size = 32,
                TileProjection projection = TileProjection::Orthogonal);

        [[nodiscard]] auto IsValid() const noexcept -> bool;
        [[nodiscard]] auto TileSize() const noexcept -> algo::Vec2 { return tile_size_; }
        [[nodiscard]] auto ChunkSize() const noexcept -> std::uint32_t { return chunk_size_; }
        [[nodiscard]] auto Projection() const noexcept -> TileProjection { return projection_; }
        [[nodiscard]] auto Id() const noexcept -> std::string_view { return id_; }
        auto SetId(std::string id) -> void { id_ = std::move(id); }

        // Definitions may be shared by many layers/chunks. Replacing a definition
        // changes its appearance/collision everywhere without rewriting cells.
        [[nodiscard]] auto DefineTile(TileDefinition definition) -> bool;
        [[nodiscard]] auto FindTile(std::uint32_t id) const -> const TileDefinition*;

        [[nodiscard]] auto AddTileLayer(std::string name, std::int32_t draw_order = 0,
                                        bool collision_enabled = false) -> std::size_t;
        [[nodiscard]] auto SetParallax(std::size_t layer, algo::Vec2 factor) -> bool;
        [[nodiscard]] auto AddObjectLayer(std::string name) -> std::size_t;
        [[nodiscard]] auto SetCell(std::size_t layer, std::int32_t x, std::int32_t y, std::uint32_t tile_id) -> bool;
        [[nodiscard]] auto GetCell(std::size_t layer, std::int32_t x, std::int32_t y) const -> std::uint32_t;
        [[nodiscard]] auto RemoveChunk(std::size_t layer, ChunkCoord chunk) -> bool;
        [[nodiscard]] auto AddObject(std::size_t layer, MapObject object) -> bool;

        [[nodiscard]] auto TileLayers() const noexcept -> const std::vector<TileLayer>& { return tile_layers_; }
        [[nodiscard]] auto ObjectLayers() const noexcept -> const std::vector<ObjectLayer>& { return object_layers_; }

        using CellVisitor = std::function<void(std::int32_t x, std::int32_t y,
                                               const TileDefinition& definition)>;
        // Visits only occupied cells intersecting the world rectangle. Sparse
        // chunk lookup makes the cost proportional to the visible area.
        auto ForEachCell(std::size_t layer, algo::Rect world_bounds, const CellVisitor& visitor) const -> void;

        // Greedy rectangle merge per chunk/layer. Colliders never cross chunk
        // boundaries, so a streamed chunk can be added or removed independently.
        [[nodiscard]] auto BuildColliders() const -> std::vector<TileCollider>;

    private:
        [[nodiscard]] auto ChunkOf(std::int32_t cell) const noexcept -> std::int32_t;
        [[nodiscard]] auto LocalOf(std::int32_t cell) const noexcept -> std::uint32_t;

        algo::Vec2 tile_size_{};
        std::uint32_t chunk_size_ = 32;
        TileProjection projection_ = TileProjection::Orthogonal;
        std::string id_{};
        std::unordered_map<std::uint32_t, TileDefinition> definitions_{};
        std::vector<TileLayer> tile_layers_{};
        std::vector<ObjectLayer> object_layers_{};
};

} // namespace atom::world2d

#endif // ATOM_WORLD2D_TILEMAP_HPP

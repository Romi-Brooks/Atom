// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <World2D/Tilemap.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace atom::world2d {

namespace {

[[nodiscard]] auto PositiveFinite(float value) -> bool { return std::isfinite(value) && value > 0.0f; }

[[nodiscard]] auto FloorToCell(double value) -> std::int32_t {
    const double floored = std::floor(value);
    return static_cast<std::int32_t>(std::clamp(floored, static_cast<double>(std::numeric_limits<std::int32_t>::min()),
                                                static_cast<double>(std::numeric_limits<std::int32_t>::max())));
}

} // namespace

auto ChunkCoordHash::operator()(const ChunkCoord coord) const noexcept -> std::size_t {
    const auto x = static_cast<std::uint32_t>(coord.x);
    const auto y = static_cast<std::uint32_t>(coord.y);
    return static_cast<std::size_t>((static_cast<std::uint64_t>(x) << 32U) | y);
}

Tilemap::Tilemap(const algo::Vec2 tile_size, const std::uint32_t chunk_size, const TileProjection projection)
    : tile_size_(tile_size), chunk_size_(chunk_size), projection_(projection) {}

auto Tilemap::IsValid() const noexcept -> bool {
    return PositiveFinite(tile_size_.GetX()) && PositiveFinite(tile_size_.GetY()) && chunk_size_ > 0 &&
           chunk_size_ <= 256 && projection_ == TileProjection::Orthogonal;
}

auto Tilemap::DefineTile(TileDefinition definition) -> bool {
    if (!IsValid() || definition.id == 0 || definition.texture_uri.empty() ||
        !PositiveFinite(definition.source.width) || !PositiveFinite(definition.source.height) ||
        !std::isfinite(definition.source.x) || !std::isfinite(definition.source.y))
        return false;
    definitions_.insert_or_assign(definition.id, std::move(definition));
    return true;
}

auto Tilemap::FindTile(const std::uint32_t id) const -> const TileDefinition* {
    const auto found = definitions_.find(id);
    return found == definitions_.end() ? nullptr : &found->second;
}

auto Tilemap::AddTileLayer(std::string name, const std::int32_t draw_order, const bool collision_enabled)
    -> std::size_t {
    const auto index = tile_layers_.size();
    tile_layers_.push_back(TileLayer{.name = std::move(name),
                                     .draw_order = draw_order,
                                     .visible = true,
                                     .collision_enabled = collision_enabled});
    return index;
}

auto Tilemap::SetParallax(const std::size_t layer, const algo::Vec2 factor) -> bool {
    if (layer >= tile_layers_.size() || !std::isfinite(factor.GetX()) || !std::isfinite(factor.GetY()) ||
        factor.GetX() < 0.0f || factor.GetY() < 0.0f)
        return false;
    tile_layers_[layer].parallax = factor;
    return true;
}

auto Tilemap::AddObjectLayer(std::string name) -> std::size_t {
    const auto index = object_layers_.size();
    object_layers_.push_back(ObjectLayer{.name = std::move(name)});
    return index;
}

auto Tilemap::ChunkOf(const std::int32_t cell) const noexcept -> std::int32_t {
    const auto span = static_cast<std::int32_t>(chunk_size_);
    const auto quotient = cell / span;
    return cell < 0 && cell % span != 0 ? quotient - 1 : quotient;
}

auto Tilemap::LocalOf(const std::int32_t cell) const noexcept -> std::uint32_t {
    const auto span = static_cast<std::int32_t>(chunk_size_);
    const auto remainder = cell % span;
    return static_cast<std::uint32_t>(remainder < 0 ? remainder + span : remainder);
}

auto Tilemap::SetCell(const std::size_t layer, const std::int32_t x, const std::int32_t y,
                      const std::uint32_t tile_id) -> bool {
    if (!IsValid() || layer >= tile_layers_.size() || (tile_id != 0 && FindTile(tile_id) == nullptr))
        return false;
    auto& chunks = tile_layers_[layer].chunks;
    const ChunkCoord coord{ChunkOf(x), ChunkOf(y)};
    auto found = chunks.find(coord);
    if (found == chunks.end()) {
        if (tile_id == 0)
            return true;
        TileChunk chunk{};
        chunk.coord = coord;
        chunk.cells.resize(static_cast<std::size_t>(chunk_size_) * chunk_size_, 0);
        found = chunks.emplace(coord, std::move(chunk)).first;
    }
    found->second.cells[static_cast<std::size_t>(LocalOf(y)) * chunk_size_ + LocalOf(x)] = tile_id;
    return true;
}

auto Tilemap::GetCell(const std::size_t layer, const std::int32_t x, const std::int32_t y) const -> std::uint32_t {
    if (!IsValid() || layer >= tile_layers_.size())
        return 0;
    const auto& chunks = tile_layers_[layer].chunks;
    const auto found = chunks.find(ChunkCoord{ChunkOf(x), ChunkOf(y)});
    if (found == chunks.end())
        return 0;
    return found->second.cells[static_cast<std::size_t>(LocalOf(y)) * chunk_size_ + LocalOf(x)];
}

auto Tilemap::RemoveChunk(const std::size_t layer, const ChunkCoord chunk) -> bool {
    return layer < tile_layers_.size() && tile_layers_[layer].chunks.erase(chunk) != 0;
}

auto Tilemap::AddObject(const std::size_t layer, MapObject object) -> bool {
    if (layer >= object_layers_.size() || object.stable_id == 0 || object.prefab_uri.empty() ||
        !std::isfinite(object.position.GetX()) || !std::isfinite(object.position.GetY()) ||
        !std::isfinite(object.rotation))
        return false;
    for (const auto& item_layer : object_layers_) {
        for (const auto& item : item_layer.objects) {
            if (item.stable_id == object.stable_id)
                return false;
        }
    }
    object_layers_[layer].objects.push_back(std::move(object));
    return true;
}

auto Tilemap::ForEachCell(const std::size_t layer, const algo::Rect world_bounds, const CellVisitor& visitor) const -> void {
    if (!IsValid() || layer >= tile_layers_.size() || !tile_layers_[layer].visible || !visitor ||
        !std::isfinite(world_bounds.x) || !std::isfinite(world_bounds.y) ||
        !PositiveFinite(world_bounds.width) || !PositiveFinite(world_bounds.height))
        return;
    const auto min_x = FloorToCell(static_cast<double>(world_bounds.x) / tile_size_.GetX());
    const auto min_y = FloorToCell(static_cast<double>(world_bounds.y) / tile_size_.GetY());
    const auto max_x = FloorToCell(std::nextafter((static_cast<double>(world_bounds.x) + world_bounds.width) /
                                                     tile_size_.GetX(), -std::numeric_limits<double>::infinity()));
    const auto max_y = FloorToCell(std::nextafter((static_cast<double>(world_bounds.y) + world_bounds.height) /
                                                     tile_size_.GetY(), -std::numeric_limits<double>::infinity()));
    // Choose the cheaper direction: probe a small visible chunk rectangle, or
    // scan the loaded set for an exceptionally large view. Sort either way so
    // draw order is stable despite unordered storage.
    const auto first_chunk_x = ChunkOf(min_x);
    const auto first_chunk_y = ChunkOf(min_y);
    const auto last_chunk_x = ChunkOf(max_x);
    const auto last_chunk_y = ChunkOf(max_y);
    const auto& chunks = tile_layers_[layer].chunks;
    std::vector<const TileChunk*> visible_chunks{};
    const auto span_x = static_cast<std::int64_t>(last_chunk_x) - first_chunk_x + 1;
    const auto span_y = static_cast<std::int64_t>(last_chunk_y) - first_chunk_y + 1;
    const auto probe_limit = static_cast<std::int64_t>(chunks.size()) * 2 + 128;
    if (span_x > 0 && span_y > 0 && span_x <= probe_limit / span_y) {
        for (std::int64_t y = first_chunk_y; y <= last_chunk_y; ++y)
            for (std::int64_t x = first_chunk_x; x <= last_chunk_x; ++x)
                if (const auto found = chunks.find(ChunkCoord{static_cast<std::int32_t>(x),
                                                                static_cast<std::int32_t>(y)}); found != chunks.end())
                    visible_chunks.push_back(&found->second);
    } else {
        for (const auto& [coord, chunk] : chunks)
            if (coord.x >= first_chunk_x && coord.x <= last_chunk_x &&
                coord.y >= first_chunk_y && coord.y <= last_chunk_y)
                visible_chunks.push_back(&chunk);
    }
    std::sort(visible_chunks.begin(), visible_chunks.end(), [](const TileChunk* a, const TileChunk* b) {
        if (a->coord.y != b->coord.y)
            return a->coord.y < b->coord.y;
        return a->coord.x < b->coord.x;
    });
    for (const auto* visible_chunk : visible_chunks) {
            const auto& chunk = *visible_chunk;
            const auto coord = chunk.coord;
            if (coord.x < first_chunk_x || coord.x > last_chunk_x || coord.y < first_chunk_y || coord.y > last_chunk_y)
                continue;
            for (std::uint32_t y = 0; y < chunk_size_; ++y) {
                const auto world_y = static_cast<std::int64_t>(coord.y) * chunk_size_ + y;
                if (world_y < min_y || world_y > max_y)
                    continue;
                for (std::uint32_t x = 0; x < chunk_size_; ++x) {
                    const auto world_x = static_cast<std::int64_t>(coord.x) * chunk_size_ + x;
                    if (world_x < min_x || world_x > max_x)
                        continue;
                    const auto id = chunk.cells[static_cast<std::size_t>(y) * chunk_size_ + x];
                    if (const auto* tile = FindTile(id); tile != nullptr)
                        visitor(static_cast<std::int32_t>(world_x), static_cast<std::int32_t>(world_y), *tile);
                }
            }
    }
}

auto Tilemap::BuildColliders() const -> std::vector<TileCollider> {
    std::vector<TileCollider> result{};
    if (!IsValid())
        return result;
    const auto side = static_cast<std::size_t>(chunk_size_);
    for (std::size_t layer = 0; layer < tile_layers_.size(); ++layer) {
        if (!tile_layers_[layer].collision_enabled)
            continue;
        for (const auto& [coord, chunk] : tile_layers_[layer].chunks) {
            std::vector<bool> used(side * side, false);
            const auto solid = [&](const std::size_t x, const std::size_t y) -> bool {
                const auto* definition = FindTile(chunk.cells[y * side + x]);
                return definition != nullptr && definition->solid && !used[y * side + x];
            };
            for (std::size_t y = 0; y < side; ++y) {
                for (std::size_t x = 0; x < side; ++x) {
                    if (!solid(x, y))
                        continue;
                    std::size_t width = 1;
                    while (x + width < side && solid(x + width, y))
                        ++width;
                    std::size_t height = 1;
                    while (y + height < side) {
                        bool row_solid = true;
                        for (std::size_t dx = 0; dx < width; ++dx)
                            row_solid &= solid(x + dx, y + height);
                        if (!row_solid)
                            break;
                        ++height;
                    }
                    for (std::size_t dy = 0; dy < height; ++dy)
                        for (std::size_t dx = 0; dx < width; ++dx)
                            used[(y + dy) * side + x + dx] = true;
                    const float world_x = static_cast<float>(static_cast<std::int64_t>(coord.x) * chunk_size_ + x) *
                                          tile_size_.GetX();
                    const float world_y = static_cast<float>(static_cast<std::int64_t>(coord.y) * chunk_size_ + y) *
                                          tile_size_.GetY();
                    result.push_back(TileCollider{layer, coord, {world_x, world_y,
                                                          static_cast<float>(width) * tile_size_.GetX(),
                                                          static_cast<float>(height) * tile_size_.GetY()}});
                }
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const TileCollider& a, const TileCollider& b) {
        if (a.layer_index != b.layer_index)
            return a.layer_index < b.layer_index;
        if (a.bounds.y != b.bounds.y)
            return a.bounds.y < b.bounds.y;
        return a.bounds.x < b.bounds.x;
    });
    return result;
}

} // namespace atom::world2d

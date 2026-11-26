// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Atom/World/World2D/TilemapLoader.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Atom/Resources/Filesystem/FileSystem.hpp>
#include <Atom/Core/Utilities/Json/JsonReader.hpp>

namespace atom::world2d {

namespace {

using json::Value;

[[nodiscard]] auto Invalid(const std::string_view uri, const Value& at, const std::string_view field,
                            const std::string_view message) -> Error {
    return MakeError(ErrorCode::SceneInvalidField, Module::Scene, "ParseTilemap")
        .WithResource(std::string{uri})
        .WithDetail(json::FormatPosition(at.line, at.column,
                                         std::string{field} + ": " + std::string{message}));
}

[[nodiscard]] auto CheckKeys(const Value& value, const std::initializer_list<std::string_view> allowed,
                              const std::string_view uri, const std::string_view field) -> Result<void> {
    if (!value.IsObject())
        return std::unexpected<Error>{Invalid(uri, value, field, "expected an object")};
    for (const auto& member : value.members) {
        bool found = false;
        for (const auto key : allowed)
            found |= member.key == key;
        if (!found)
            return std::unexpected<Error>{Invalid(uri, *member.value, field, "unknown field " + member.key)};
    }
    return Ok();
}

[[nodiscard]] auto Require(const Value& parent, const std::string_view key,
                            const std::string_view uri) -> Result<const Value*> {
    const auto* found = parent.Find(key);
    if (found == nullptr)
        return std::unexpected<Error>{Invalid(uri, parent, key, "required field is missing")};
    return found;
}

[[nodiscard]] auto String(const Value& value, const std::string_view uri, const std::string_view field)
    -> Result<std::string> {
    if (!value.IsString() || value.string.empty() || value.string.size() > 256)
        return std::unexpected<Error>{Invalid(uri, value, field, "expected a nonempty string of at most 256 bytes")};
    return value.string;
}

[[nodiscard]] auto Integer(const Value& value, const std::string_view uri, const std::string_view field,
                            const std::int64_t min, const std::int64_t max) -> Result<std::int64_t> {
    if (!value.IsNumber() || !std::isfinite(value.number) || std::floor(value.number) != value.number ||
        value.number < static_cast<double>(min) || value.number > static_cast<double>(max))
        return std::unexpected<Error>{Invalid(uri, value, field, "expected an integer in range")};
    return static_cast<std::int64_t>(value.number);
}

[[nodiscard]] auto Number(const Value& value, const std::string_view uri, const std::string_view field)
    -> Result<float> {
    if (!value.IsNumber() || !std::isfinite(value.number) ||
        std::abs(value.number) > std::numeric_limits<float>::max())
        return std::unexpected<Error>{Invalid(uri, value, field, "expected a finite number")};
    return static_cast<float>(value.number);
}

[[nodiscard]] auto Vec2(const Value& value, const std::string_view uri, const std::string_view field)
    -> Result<algo::Vec2> {
    if (!value.IsArray() || value.items.size() != 2)
        return std::unexpected<Error>{Invalid(uri, value, field, "expected an array of two numbers")};
    auto x = Number(*value.items[0], uri, field);
    if (!x) return std::unexpected<Error>{std::move(x.error())};
    auto y = Number(*value.items[1], uri, field);
    if (!y) return std::unexpected<Error>{std::move(y.error())};
    return algo::Vec2{*x, *y};
}

[[nodiscard]] auto Rect(const Value& value, const std::string_view uri, const std::string_view field)
    -> Result<algo::Rect> {
    if (!value.IsArray() || value.items.size() != 4)
        return std::unexpected<Error>{Invalid(uri, value, field, "expected an array of four numbers")};
    float parts[4]{};
    for (std::size_t i = 0; i < 4; ++i) {
        auto number = Number(*value.items[i], uri, field);
        if (!number) return std::unexpected<Error>{std::move(number.error())};
        parts[i] = *number;
    }
    if (parts[2] <= 0.0f || parts[3] <= 0.0f)
        return std::unexpected<Error>{Invalid(uri, value, field, "width and height must be positive")};
    return algo::Rect{parts[0], parts[1], parts[2], parts[3]};
}

} // namespace

auto ParseTilemap(const std::string_view text, const std::string_view uri) -> Result<Tilemap> {
    json::SyntaxError syntax{};
    auto document = json::TryParse(text, syntax);
    if (!document)
        return std::unexpected<Error>{MakeError(ErrorCode::SceneInvalidDocument, Module::Scene, "ParseTilemap")
                                          .WithResource(std::string{uri})
                                          .WithDetail(json::FormatPosition(syntax.line, syntax.column, syntax.message))};
    const auto& root = *document;
    if (auto keys = CheckKeys(root, {"schema_version", "kind", "id", "projection", "tile_size",
                                     "chunk_size", "tiles", "layers"}, uri, "tilemap"); !keys)
        return std::unexpected<Error>{std::move(keys.error())};
    auto version_value = Require(root, "schema_version", uri);
    if (!version_value) return std::unexpected<Error>{std::move(version_value.error())};
    auto version = Integer(**version_value, uri, "schema_version", 1, 1);
    if (!version) return std::unexpected<Error>{std::move(version.error())};
    auto kind_value = Require(root, "kind", uri);
    if (!kind_value) return std::unexpected<Error>{std::move(kind_value.error())};
    auto kind = String(**kind_value, uri, "kind");
    if (!kind) return std::unexpected<Error>{std::move(kind.error())};
    if (*kind != "tilemap")
        return std::unexpected<Error>{Invalid(uri, **kind_value, "kind", "expected tilemap")};
    auto id_value = Require(root, "id", uri);
    if (!id_value) return std::unexpected<Error>{std::move(id_value.error())};
    auto id = String(**id_value, uri, "id");
    if (!id) return std::unexpected<Error>{std::move(id.error())};
    auto projection_value = Require(root, "projection", uri);
    if (!projection_value) return std::unexpected<Error>{std::move(projection_value.error())};
    auto projection = String(**projection_value, uri, "projection");
    if (!projection) return std::unexpected<Error>{std::move(projection.error())};
    if (*projection != "orthogonal")
        return std::unexpected<Error>{Invalid(uri, **projection_value, "projection", "unsupported projection")};
    auto size_value = Require(root, "tile_size", uri);
    if (!size_value) return std::unexpected<Error>{std::move(size_value.error())};
    auto size = Vec2(**size_value, uri, "tile_size");
    if (!size) return std::unexpected<Error>{std::move(size.error())};
    auto chunk_value = Require(root, "chunk_size", uri);
    if (!chunk_value) return std::unexpected<Error>{std::move(chunk_value.error())};
    auto chunk_size = Integer(**chunk_value, uri, "chunk_size", 1, 256);
    if (!chunk_size) return std::unexpected<Error>{std::move(chunk_size.error())};
    Tilemap map{*size, static_cast<std::uint32_t>(*chunk_size)};
    if (!map.IsValid())
        return std::unexpected<Error>{Invalid(uri, **size_value, "tile_size", "tile dimensions must be positive")};
    map.SetId(*id);

    auto definitions_value = Require(root, "tiles", uri);
    if (!definitions_value) return std::unexpected<Error>{std::move(definitions_value.error())};
    if (!(**definitions_value).IsArray())
        return std::unexpected<Error>{Invalid(uri, **definitions_value, "tiles", "expected an array")};
    for (const auto& value : (**definitions_value).items) {
        if (auto keys = CheckKeys(*value, {"id", "texture", "source", "solid", "filter"}, uri, "tiles"); !keys)
            return std::unexpected<Error>{std::move(keys.error())};
        auto tile_id_value = Require(*value, "id", uri);
        if (!tile_id_value) return std::unexpected<Error>{std::move(tile_id_value.error())};
        auto tile_id = Integer(**tile_id_value, uri, "tiles.id", 1, 2147483647);
        if (!tile_id) return std::unexpected<Error>{std::move(tile_id.error())};
        auto texture_value = Require(*value, "texture", uri);
        if (!texture_value) return std::unexpected<Error>{std::move(texture_value.error())};
        auto texture = String(**texture_value, uri, "tiles.texture");
        if (!texture) return std::unexpected<Error>{std::move(texture.error())};
        fs::AssetPath parsed{};
        if (!fs::AssetPath::TryParse(*texture, parsed))
            return std::unexpected<Error>{Invalid(uri, **texture_value, "tiles.texture", "expected an asset URI")};
        auto source_value = Require(*value, "source", uri);
        if (!source_value) return std::unexpected<Error>{std::move(source_value.error())};
        auto source = Rect(**source_value, uri, "tiles.source");
        if (!source) return std::unexpected<Error>{std::move(source.error())};
        TileDefinition tile{.id = static_cast<std::uint32_t>(*tile_id), .texture_uri = *texture,
                            .source = *source};
        if (const auto* solid = value->Find("solid"); solid != nullptr) {
            if (!solid->IsBool())
                return std::unexpected<Error>{Invalid(uri, *solid, "tiles.solid", "expected a boolean")};
            tile.solid = solid->boolean;
        }
        if (const auto* filter = value->Find("filter"); filter != nullptr) {
            auto name = String(*filter, uri, "tiles.filter");
            if (!name) return std::unexpected<Error>{std::move(name.error())};
            if (*name == "linear") tile.filter = ecs::SpriteFilter::Linear;
            else if (*name != "nearest")
                return std::unexpected<Error>{Invalid(uri, *filter, "tiles.filter", "expected linear or nearest")};
        }
        if (map.FindTile(tile.id) != nullptr || !map.DefineTile(std::move(tile)))
            return std::unexpected<Error>{Invalid(uri, *value, "tiles", "duplicate or invalid tile definition")};
    }

    auto layers_value = Require(root, "layers", uri);
    if (!layers_value) return std::unexpected<Error>{std::move(layers_value.error())};
    if (!(**layers_value).IsArray())
        return std::unexpected<Error>{Invalid(uri, **layers_value, "layers", "expected an array")};
    std::unordered_set<std::string> names{};
    for (const auto& layer_value : (**layers_value).items) {
        if (auto keys = CheckKeys(*layer_value, {"kind", "name", "draw_order", "collision", "parallax",
                                                 "chunks", "objects"}, uri, "layers"); !keys)
            return std::unexpected<Error>{std::move(keys.error())};
        auto name_value = Require(*layer_value, "name", uri);
        if (!name_value) return std::unexpected<Error>{std::move(name_value.error())};
        auto name = String(**name_value, uri, "layers.name");
        if (!name) return std::unexpected<Error>{std::move(name.error())};
        if (!names.insert(*name).second)
            return std::unexpected<Error>{Invalid(uri, **name_value, "layers.name", "duplicate layer name")};
        auto layer_kind_value = Require(*layer_value, "kind", uri);
        if (!layer_kind_value) return std::unexpected<Error>{std::move(layer_kind_value.error())};
        auto layer_kind = String(**layer_kind_value, uri, "layers.kind");
        if (!layer_kind) return std::unexpected<Error>{std::move(layer_kind.error())};
        if (*layer_kind == "tiles") {
            if (layer_value->Find("objects") != nullptr)
                return std::unexpected<Error>{Invalid(uri, *layer_value, "layers.objects", "not valid on tile layers")};
            std::int32_t draw_order = 0;
            if (const auto* field = layer_value->Find("draw_order"); field != nullptr) {
                auto value = Integer(*field, uri, "layers.draw_order", -1000000, 1000000);
                if (!value) return std::unexpected<Error>{std::move(value.error())};
                draw_order = static_cast<std::int32_t>(*value);
            }
            bool collision = false;
            if (const auto* field = layer_value->Find("collision"); field != nullptr) {
                if (!field->IsBool())
                    return std::unexpected<Error>{Invalid(uri, *field, "layers.collision", "expected boolean")};
                collision = field->boolean;
            }
            const auto index = map.AddTileLayer(*name, draw_order, collision);
            if (const auto* field = layer_value->Find("parallax"); field != nullptr) {
                auto factor = Vec2(*field, uri, "layers.parallax");
                if (!factor) return std::unexpected<Error>{std::move(factor.error())};
                if (!map.SetParallax(index, *factor))
                    return std::unexpected<Error>{Invalid(uri, *field, "layers.parallax", "must be nonnegative")};
            }
            auto chunks_value = Require(*layer_value, "chunks", uri);
            if (!chunks_value) return std::unexpected<Error>{std::move(chunks_value.error())};
            if (!(**chunks_value).IsArray())
                return std::unexpected<Error>{Invalid(uri, **chunks_value, "layers.chunks", "expected array")};
            std::unordered_set<ChunkCoord, ChunkCoordHash> seen_chunks{};
            for (const auto& chunk : (**chunks_value).items) {
                if (auto keys = CheckKeys(*chunk, {"x", "y", "cells"}, uri, "chunks"); !keys)
                    return std::unexpected<Error>{std::move(keys.error())};
                auto x_value = Require(*chunk, "x", uri);
                if (!x_value) return std::unexpected<Error>{std::move(x_value.error())};
                auto x = Integer(**x_value, uri, "chunks.x", -1000000, 1000000);
                if (!x) return std::unexpected<Error>{std::move(x.error())};
                auto y_value = Require(*chunk, "y", uri);
                if (!y_value) return std::unexpected<Error>{std::move(y_value.error())};
                auto y = Integer(**y_value, uri, "chunks.y", -1000000, 1000000);
                if (!y) return std::unexpected<Error>{std::move(y.error())};
                const ChunkCoord coord{static_cast<std::int32_t>(*x), static_cast<std::int32_t>(*y)};
                if (!seen_chunks.insert(coord).second)
                    return std::unexpected<Error>{Invalid(uri, *chunk, "chunks", "duplicate chunk coordinate")};
                auto cells_value = Require(*chunk, "cells", uri);
                if (!cells_value) return std::unexpected<Error>{std::move(cells_value.error())};
                const auto& cells = **cells_value;
                const auto count = static_cast<std::size_t>(*chunk_size) * static_cast<std::size_t>(*chunk_size);
                if (!cells.IsArray() || cells.items.size() != count)
                    return std::unexpected<Error>{Invalid(uri, cells, "chunks.cells", "expected chunk_size squared entries")};
                for (std::size_t cell = 0; cell < count; ++cell) {
                    auto tile_id = Integer(*cells.items[cell], uri, "chunks.cells", 0, 2147483647);
                    if (!tile_id) return std::unexpected<Error>{std::move(tile_id.error())};
                    const auto world_x = static_cast<std::int32_t>(*x * *chunk_size + cell % *chunk_size);
                    const auto world_y = static_cast<std::int32_t>(*y * *chunk_size + cell / *chunk_size);
                    if (!map.SetCell(index, world_x, world_y, static_cast<std::uint32_t>(*tile_id)))
                        return std::unexpected<Error>{Invalid(uri, *cells.items[cell], "chunks.cells", "undefined tile id")};
                }
            }
        } else if (*layer_kind == "objects") {
            if (layer_value->Find("chunks") != nullptr || layer_value->Find("collision") != nullptr ||
                layer_value->Find("parallax") != nullptr || layer_value->Find("draw_order") != nullptr)
                return std::unexpected<Error>{Invalid(uri, *layer_value, "layers", "tile fields on object layer")};
            const auto index = map.AddObjectLayer(*name);
            auto objects_value = Require(*layer_value, "objects", uri);
            if (!objects_value) return std::unexpected<Error>{std::move(objects_value.error())};
            if (!(**objects_value).IsArray())
                return std::unexpected<Error>{Invalid(uri, **objects_value, "layers.objects", "expected array")};
            for (const auto& object : (**objects_value).items) {
                if (auto keys = CheckKeys(*object, {"id", "prefab", "position", "rotation"}, uri, "objects"); !keys)
                    return std::unexpected<Error>{std::move(keys.error())};
                auto object_id_value = Require(*object, "id", uri);
                if (!object_id_value) return std::unexpected<Error>{std::move(object_id_value.error())};
                auto object_id = Integer(**object_id_value, uri, "objects.id", 1, 2147483647);
                if (!object_id) return std::unexpected<Error>{std::move(object_id.error())};
                auto prefab_value = Require(*object, "prefab", uri);
                if (!prefab_value) return std::unexpected<Error>{std::move(prefab_value.error())};
                auto prefab = String(**prefab_value, uri, "objects.prefab");
                if (!prefab) return std::unexpected<Error>{std::move(prefab.error())};
                fs::AssetPath parsed{};
                if (!fs::AssetPath::TryParse(*prefab, parsed))
                    return std::unexpected<Error>{Invalid(uri, **prefab_value, "objects.prefab", "expected asset URI")};
                auto position_value = Require(*object, "position", uri);
                if (!position_value) return std::unexpected<Error>{std::move(position_value.error())};
                auto position = Vec2(**position_value, uri, "objects.position");
                if (!position) return std::unexpected<Error>{std::move(position.error())};
                float rotation = 0.0f;
                if (const auto* field = object->Find("rotation"); field != nullptr) {
                    auto value = Number(*field, uri, "objects.rotation");
                    if (!value) return std::unexpected<Error>{std::move(value.error())};
                    rotation = *value;
                }
                if (!map.AddObject(index, MapObject{static_cast<std::uint64_t>(*object_id), *prefab,
                                                     *position, rotation}))
                    return std::unexpected<Error>{Invalid(uri, *object, "objects", "duplicate or invalid object")};
            }
        } else {
            return std::unexpected<Error>{Invalid(uri, **layer_kind_value, "layers.kind", "expected tiles or objects")};
        }
    }
    return map;
}

auto LoadTilemap(const fs::IFileSystem& filesystem, const fs::AssetPath& path) -> Result<Tilemap> {
    std::unique_ptr<fs::IFile> file{};
    if (filesystem.OpenRead(path, file) != fs::Result::Success || !file)
        return std::unexpected<Error>{MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "LoadTilemap")
                                          .WithResource(path.String()).WithDetail("could not open tilemap")};
    std::vector<std::byte> bytes{};
    if (fs::ReadAll(*file, bytes) != fs::Result::Success)
        return std::unexpected<Error>{MakeError(ErrorCode::SceneLoadFailed, Module::Scene, "LoadTilemap")
                                          .WithResource(path.String()).WithDetail("could not read tilemap")};
    return ParseTilemap(std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()}, path.String());
}

} // namespace atom::world2d

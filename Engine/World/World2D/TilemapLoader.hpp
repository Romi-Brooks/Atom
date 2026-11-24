// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file TilemapLoader.hpp
 * @brief Strict JSONC and VFS loading for chunk-addressed tilemaps.
 */
#ifndef ATOM_WORLD2D_TILEMAPLOADER_HPP
#define ATOM_WORLD2D_TILEMAPLOADER_HPP

#include <string_view>

#include <Error/Error.hpp>
#include <Filesystem/AssetPath.hpp>
#include <World2D/Tilemap.hpp>

namespace atom::fs { class IFileSystem; }

namespace atom::world2d {

// Format v1: orthogonal tile_size/chunk_size, tile definitions, tile layers
// with fixed-size chunk cell arrays, and object layers with stable prefab IDs.
[[nodiscard]] auto ParseTilemap(std::string_view text, std::string_view uri) -> Result<Tilemap>;
[[nodiscard]] auto LoadTilemap(const fs::IFileSystem& filesystem, const fs::AssetPath& path) -> Result<Tilemap>;

} // namespace atom::world2d

#endif // ATOM_WORLD2D_TILEMAPLOADER_HPP

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file SpriteAtlasLoader.hpp
 * @brief Reads an atlas document through the VFS, with a per-load cache.
 *
 * Why a cache and not a singleton: an atlas is referenced once per frame name, so a
 * scene with thirty frames of one sheet would otherwise parse that sheet thirty times.
 * Why per-load and not per-process: an artist edits a sheet, reloads the level, and
 * expects to see the change - a process-lifetime cache turns that into "why is my
 * atlas change ignored", which is a worse bug than the parse it saves.
 */

#ifndef ATOM_SPRITE_SPRITEATLASLOADER_HPP
#define ATOM_SPRITE_SPRITEATLASLOADER_HPP

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Error/Error.hpp>
#include <Filesystem/AssetPath.hpp>
#include <Sprite/SpriteAtlas.hpp>

namespace atom::fs {
class IFileSystem;
} // namespace atom::fs

namespace atom::sprite {

class SpriteAtlasLoader final {
    public:
        // `filesystem` must outlive the loader.
        explicit SpriteAtlasLoader(const fs::IFileSystem& filesystem) : filesystem_(&filesystem) {}

        // Returns a pointer that stays valid for the lifetime of the loader (the loader
        // owns the atlases; a pointer, not a reference, because std::expected cannot
        // hold a reference), or an Error whose detail starts with the atlas's own
        // "uri:line:col:" diagnostic.
        [[nodiscard]] auto Load(const fs::AssetPath& path) -> Result<const SpriteAtlas*>;
        [[nodiscard]] auto Load(std::string_view uri) -> Result<const SpriteAtlas*>;

        [[nodiscard]] auto LoadedCount() const noexcept -> std::size_t {
            return cache_.size();
        }

    private:
        const fs::IFileSystem* filesystem_ = nullptr;
        // unique_ptr so a reference handed out stays valid while later inserts happen.
        std::unordered_map<std::string, std::unique_ptr<SpriteAtlas>> cache_{};
};

} // namespace atom::sprite

#endif // ATOM_SPRITE_SPRITEATLASLOADER_HPP

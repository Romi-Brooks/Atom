// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file TextureCacheResolver.hpp
 * @brief The production ITextureResolver: asset URIs backed by TextureCache.
 *
 * Kept separate from Scene2DRenderer so the collector has no resource-system
 * dependency at all: the renderer takes the abstraction, and this adapter is the
 * only place that knows about VFS, AssetPath and GPU handles.
 *
 * Two behaviours matter for a game loop:
 *
 *  - memoisation. Resolve() is called once per sprite per frame, so a hit must not
 *    touch the filesystem again, and a miss must not either: a missing texture
 *    would otherwise re-open the VFS (and log) 60 times a second.
 *  - ownership. The resolver holds the TextureHandle, so the texture stays alive
 *    while a Scene2DRenderer can still draw it. Dropping the resolver releases
 *    them; TextureCache then defers the GPU destroy to the frame boundary.
 */

#ifndef ATOM_RENDER_SCENE2D_TEXTURECACHERESOLVER_HPP
#define ATOM_RENDER_SCENE2D_TEXTURECACHERESOLVER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <Render/Resources/TextureCache.hpp>
#include <Render/Scene2D/Scene2DTypes.hpp>

namespace atom::fs {
class IFileSystem;
}

namespace atom::render::scene2d {

class TextureCacheResolver final : public ITextureResolver {
    public:
        // `filesystem` must outlive the resolver; the documented convention is the
        // mounted VFS (or a test's MemoryFileSystem).
        TextureCacheResolver(resources::TextureCache& cache, const fs::IFileSystem& filesystem);
        ~TextureCacheResolver() override = default;

        TextureCacheResolver(const TextureCacheResolver&) = delete;
        auto operator=(const TextureCacheResolver&) -> TextureCacheResolver& = delete;

        // Resolves a URI once. Returns nullptr for an unparseable URI, a read
        // failure, or a decode failure, and remembers the failure so the next call
        // is free.
        [[nodiscard]] auto Resolve(std::string_view uri) -> Renderer2D::Texture* override;

        [[nodiscard]] auto ResolvedCount() const noexcept -> std::size_t {
            return resolved_.size();
        }
        [[nodiscard]] auto FailedCount() const noexcept -> std::size_t {
            return failed_.size();
        }

        // Drops every handle this resolver holds. The cache may then evict them at
        // the next frame boundary.
        auto ReleaseAll() -> void;

    private:
        resources::TextureCache* cache_ = nullptr;
        const fs::IFileSystem* filesystem_ = nullptr;
        std::unordered_map<std::string, resources::TextureHandle> resolved_{};
        std::unordered_set<std::string> failed_{};
};

} // namespace atom::render::scene2d

#endif // ATOM_RENDER_SCENE2D_TEXTURECACHERESOLVER_HPP

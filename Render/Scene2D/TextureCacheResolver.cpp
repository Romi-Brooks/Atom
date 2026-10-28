// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Render/Scene2D/TextureCacheResolver.hpp>

#include <Filesystem/AssetPath.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Log/LogSystem.hpp>

namespace atom::render::scene2d {

TextureCacheResolver::TextureCacheResolver(resources::TextureCache& cache, const fs::IFileSystem& filesystem)
    : cache_(&cache), filesystem_(&filesystem) {}

auto TextureCacheResolver::Resolve(const std::string_view uri) -> Renderer2D::Texture* {
    if (uri.empty() || cache_ == nullptr || filesystem_ == nullptr)
        return nullptr;

    const std::string key{uri};

    // A remembered failure costs one hash lookup, never another open/decode.
    if (failed_.contains(key))
        return nullptr;

    if (const auto found = resolved_.find(key); found != resolved_.end()) {
        if (found->second.IsValid())
            return found->second.Get();
        // The handle went inert (renderer shut down and re-initialised): forget it
        // and try again in the new generation instead of handing back nullptr
        // forever.
        resolved_.erase(found);
    }

    fs::AssetPath path{};
    if (!fs::AssetPath::TryParse(uri, path)) {
        failed_.insert(key);
        LOG_WARNING(atom::log::render::Renderer2D, "Scene2D: unparseable sprite URI '" + key + "'");
        return nullptr;
    }

    auto handle = cache_->AcquireFromFilesystem(*filesystem_, path);
    if (!handle.IsValid()) {
        failed_.insert(key);
        return nullptr;
    }

    auto* texture = handle.Get();
    resolved_.emplace(key, std::move(handle));
    return texture;
}

auto TextureCacheResolver::ReleaseAll() -> void {
    resolved_.clear();
    failed_.clear();
}

} // namespace atom::render::scene2d

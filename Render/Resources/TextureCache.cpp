/**
 * @file           : TextureCache.cpp
 * @brief          : TextureCache implementation.
**/

#include "TextureCache.hpp"

#include <utility>
#include <vector>

#include <Log/LogSystem.hpp>
#include <Media/Image/ImageDecoder.hpp>

namespace atom::render::resources {

TextureCache::TextureCache(Renderer2D& renderer) : renderer_(&renderer) {}

TextureCache::~TextureCache() = default;

auto TextureCache::AcquireFromFilesystem(const fs::IFileSystem& filesystem, const fs::AssetPath& path)
    -> TextureHandle {
    if (!renderer_ || !path.IsValid())
        return {};

    const std::string key{path.String()};
    if (const auto it = cache_.find(key); it != cache_.end()) {
        if (auto existing = it->second.lock())
            return TextureHandle{std::move(existing)};
        cache_.erase(it);
    }

    std::unique_ptr<fs::IFile> file{};
    if (filesystem.OpenRead(path, file) != fs::Result::Success || !file)
        return {};
    std::vector<std::byte> bytes{};
    if (fs::ReadAll(*file, bytes) != fs::Result::Success)
        return {};

    const auto decoded = image::DecodeImageMemory(bytes);
    return AcquireDecoded(key, decoded);
}

auto TextureCache::AcquireFromEncodedMemory(const std::string_view cache_key, const std::span<const std::byte> encoded)
    -> TextureHandle {
    if (!renderer_ || cache_key.empty() || encoded.empty())
        return {};

    const std::string key{cache_key};
    if (const auto it = cache_.find(key); it != cache_.end()) {
        if (auto existing = it->second.lock())
            return TextureHandle{std::move(existing)};
        cache_.erase(it);
    }

    return AcquireDecoded(key, image::DecodeImageMemory(encoded));
}

auto TextureCache::AcquireDecoded(const std::string_view cache_key, const image::DecodedImage& decoded)
    -> TextureHandle {
    if (!decoded.IsValid())
        return {};

    Renderer2D::Texture* texture = renderer_->CreateTexture(decoded.width, decoded.height, decoded.rgba.data());
    if (!texture)
        return {};

    auto record = std::make_shared<detail::GpuTextureRecord>();
    record->renderer = renderer_;
    record->texture = texture;
    cache_[std::string{cache_key}] = record;
    SweepExpired();
    return TextureHandle{std::move(record)};
}

auto TextureCache::Evict(const std::string& cache_key) -> bool {
    const auto it = cache_.find(cache_key);
    if (it == cache_.end())
        return false;
    cache_.erase(it);
    return true;
}

auto TextureCache::EvictAll() -> void {
    cache_.clear();
}

auto TextureCache::FlushDeferredDestroys() -> void {
    if (renderer_)
        renderer_->FlushDeferredTextureDestroys();
}

auto TextureCache::LiveCount() const -> std::size_t {
    std::size_t live = 0;
    for (const auto& [key, weak] : cache_) {
        if (!weak.expired())
            ++live;
    }
    return live;
}

auto TextureCache::CacheCapacity() const -> std::size_t {
    return cache_.size();
}

auto TextureCache::SweepExpired() -> void {
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.expired())
            it = cache_.erase(it);
        else
            ++it;
    }
}

} // namespace atom::render::resources

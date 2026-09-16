/**
 * @file           : TextureCache.hpp
 * @brief          : Shareable GPU textures with frame-boundary deferred destroy.
**/

#ifndef ATOM_RENDER_RESOURCES_TEXTURE_CACHE_HPP
#define ATOM_RENDER_RESOURCES_TEXTURE_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Filesystem/AssetPath.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

namespace atom::render::resources {

namespace detail {

struct GpuTextureRecord final {
        Renderer2D* renderer = nullptr;
        Renderer2D::Texture* texture = nullptr;

        GpuTextureRecord() = default;
        ~GpuTextureRecord() {
            // Last handle release must not call DestroyTexture directly: the
            // release may happen mid-frame. Enqueue and let the owner flush.
            if (renderer && texture)
                renderer->EnqueueDeferredTextureDestroy(texture);
        }

        GpuTextureRecord(const GpuTextureRecord&) = delete;
        GpuTextureRecord& operator=(const GpuTextureRecord&) = delete;
};

} // namespace detail

// Copyable handle to a cached GPU texture. The underlying Renderer2D::Texture
// stays alive until the last handle is dropped AND FlushDeferredTextureDestroys
// runs outside a frame.
class TextureHandle final {
    public:
        TextureHandle() = default;
        explicit TextureHandle(std::shared_ptr<detail::GpuTextureRecord> record) : record_(std::move(record)) {}

        [[nodiscard]] auto IsValid() const -> bool {
            return record_ && record_->texture != nullptr;
        }
        [[nodiscard]] auto Get() const -> Renderer2D::Texture* {
            return record_ ? record_->texture : nullptr;
        }
        [[nodiscard]] auto GetWidth() const -> uint32_t {
            return IsValid() ? record_->texture->GetWidth() : 0;
        }
        [[nodiscard]] auto GetHeight() const -> uint32_t {
            return IsValid() ? record_->texture->GetHeight() : 0;
        }
        explicit operator bool() const {
            return IsValid();
        }

    private:
        std::shared_ptr<detail::GpuTextureRecord> record_{};
};

// Identity-keyed GPU texture cache layered on top of CPU image decoding.
// Same cache key returns the same live texture. No LRU: eviction is only
// handle-driven (last release -> deferred destroy).
//
// Call FlushDeferredDestroys() once per frame outside Renderer2D::BeginFrame.
// Acquire is not internally synchronized (same contract as ResourceManager).
class TextureCache final {
    public:
        explicit TextureCache(Renderer2D& renderer);
        ~TextureCache();

        TextureCache(const TextureCache&) = delete;
        TextureCache& operator=(const TextureCache&) = delete;

        // Reads encoded image bytes through the VFS, decodes, uploads, caches.
        [[nodiscard]] auto AcquireFromFilesystem(const fs::IFileSystem& filesystem, const fs::AssetPath& path)
            -> TextureHandle;

        // Decodes an already-held encoded buffer (embedded artwork, tests).
        [[nodiscard]] auto AcquireFromEncodedMemory(std::string_view cache_key, std::span<const std::byte> encoded)
            -> TextureHandle;

        auto Evict(const std::string& cache_key) -> bool;
        auto EvictAll() -> void;
        auto FlushDeferredDestroys() -> void;

        [[nodiscard]] auto LiveCount() const -> std::size_t;
        [[nodiscard]] auto CacheCapacity() const -> std::size_t;

    private:
        [[nodiscard]] auto AcquireDecoded(std::string_view cache_key, const image::DecodedImage& decoded)
            -> TextureHandle;
        auto SweepExpired() -> void;

        Renderer2D* renderer_ = nullptr;
        std::unordered_map<std::string, std::weak_ptr<detail::GpuTextureRecord>> cache_{};
};

} // namespace atom::render::resources

#endif // ATOM_RENDER_RESOURCES_TEXTURE_CACHE_HPP

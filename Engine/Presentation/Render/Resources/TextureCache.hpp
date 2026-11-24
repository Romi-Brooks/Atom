/**
 * @file           : TextureCache.hpp
 * @brief          : Shareable GPU textures with frame-boundary deferred destroy.
**/

#ifndef ATOM_RENDER_RESOURCES_TEXTURE_CACHE_HPP
#define ATOM_RENDER_RESOURCES_TEXTURE_CACHE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Concurrency/Future.hpp>
#include <Filesystem/AssetPath.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

namespace atom::render::resources {

class TextureCache;

namespace detail {

struct GpuTextureRecord final {
        Renderer2D* renderer = nullptr;
        Renderer2D::Texture* texture = nullptr;
        // Pool generation this texture belongs to. It keeps the renderer's
        // liveness flag alive, so a handle released after Renderer2D::Shutdown()
        // (or after the renderer itself is gone) can tell that `texture` is no
        // longer a live object instead of dereferencing freed memory.
        std::shared_ptr<Renderer2D::TexturePoolState> pool{};

        GpuTextureRecord() = default;
        ~GpuTextureRecord() {
            // Last handle release must not call DestroyTexture directly: the
            // release may happen mid-frame. Enqueue and let the owner flush.
            //
            // The pool check comes first: after Shutdown() `texture` points at
            // freed storage and `renderer` may be gone as well, so neither may
            // be touched in that state.
            if (texture && IsPoolAlive() && renderer)
                renderer->EnqueueDeferredTextureDestroy(texture);
        }

        [[nodiscard]] auto IsPoolAlive() const -> bool {
            return pool != nullptr && pool->alive;
        }

        GpuTextureRecord(const GpuTextureRecord&) = delete;
        GpuTextureRecord& operator=(const GpuTextureRecord&) = delete;
};

} // namespace detail

// Copyable handle to a cached GPU texture. The underlying Renderer2D::Texture
// stays alive until the last handle is dropped AND FlushDeferredTextureDestroys
// runs outside a frame.
//
// A handle may outlive Renderer2D::Shutdown(): it then reports itself invalid
// and Get() returns nullptr instead of a dangling pointer, so callers that hold
// cached textures across a renderer teardown stay safe.
class TextureHandle final {
    public:
        TextureHandle() = default;
        explicit TextureHandle(std::shared_ptr<detail::GpuTextureRecord> record) : record_(std::move(record)) {}

        [[nodiscard]] auto IsValid() const -> bool {
            return record_ && record_->IsPoolAlive() && record_->texture != nullptr;
        }
        [[nodiscard]] auto Get() const -> Renderer2D::Texture* {
            return IsValid() ? record_->texture : nullptr;
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

namespace detail {

// Shared state behind an in-flight asynchronous texture load (ARCH-107). The
// worker (or the inline fallback) writes the CPU-side DecodedImage and sets
// `decode_done`; the render thread finalizes it through
// TextureCache::PumpAsync(), which materializes the image into a cached GPU
// texture and completes the Future. The worker never touches the renderer or
// the cache — those stay render-thread-only.
struct AsyncTextureLoad final {
        std::string key;
        std::shared_ptr<async::Promise<TextureHandle>> promise;
        std::mutex mutex;
        atom::image::DecodedImage decoded{}; // worker writes under `mutex`
        std::atomic<bool> decode_done{false};
};

} // namespace detail

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

        // Uploads an image which has already been decoded by a worker. This
        // keeps CPU decoding out of the render thread and lets several
        // consumers (thumbnail generation, colour analysis, UI) share one
        // DecodedImage rather than decoding the same encoded bytes repeatedly.
        // Like every TextureCache acquire, this is render-thread-only.
        [[nodiscard]] auto AcquireFromDecodedImage(std::string_view cache_key, const image::DecodedImage& decoded)
            -> TextureHandle;

        // Asynchronous single-image loads (ARCH-107). Decoding (read + decode
        // for the filesystem form) runs on a job-system worker; the decoded
        // image is materialized into the cache by the render thread when
        // PumpAsync() runs, which completes the returned Future. Poll
        // Future::IsReady() each frame; once Ready, Future::Get() yields the
        // TextureHandle (an empty handle means the decode/upload failed, the
        // same failure convention as the synchronous Acquire* entry points).
        //
        // An id that is already cached returns an immediately-Ready future; a
        // request that cannot even start (no renderer, empty key/bytes, invalid
        // path) returns an immediately-Ready future carrying an empty handle.
        // The filesystem reference must outlive the request.
        [[nodiscard]] auto LoadAsync(const fs::IFileSystem& filesystem, const fs::AssetPath& path)
            -> async::Future<TextureHandle>;
        [[nodiscard]] auto LoadAsync(std::string_view cache_key, std::span<const std::byte> encoded)
            -> async::Future<TextureHandle>;

        // Drains pending async loads: materializes finished decodes into cached
        // GPU textures and completes their futures. Render-thread-only; call
        // once per frame, outside Renderer2D::BeginFrame, alongside
        // FlushDeferredDestroys(). Without this, futures never become Ready.
        auto PumpAsync() -> void;

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
        std::unordered_map<std::string, std::shared_ptr<detail::AsyncTextureLoad>> in_flight_{};
};

} // namespace atom::render::resources

#endif // ATOM_RENDER_RESOURCES_TEXTURE_CACHE_HPP

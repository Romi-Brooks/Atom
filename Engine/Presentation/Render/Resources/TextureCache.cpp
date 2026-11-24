/**
 * @file           : TextureCache.cpp
 * @brief          : TextureCache implementation.
**/

#include "TextureCache.hpp"

#include <utility>
#include <vector>

#include <Concurrency/JobSystem.hpp>
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

auto TextureCache::AcquireFromDecodedImage(const std::string_view cache_key, const image::DecodedImage& decoded)
    -> TextureHandle {
    if (!renderer_ || cache_key.empty() || !decoded.IsValid())
        return {};

    const std::string key{cache_key};
    if (const auto it = cache_.find(key); it != cache_.end()) {
        if (auto existing = it->second.lock())
            return TextureHandle{std::move(existing)};
        cache_.erase(it);
    }
    return AcquireDecoded(key, decoded);
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
    record->pool = renderer_->GetTexturePoolState();
    record->texture = texture;
    cache_[std::string{cache_key}] = record;
    SweepExpired();
    return TextureHandle{std::move(record)};
}

auto TextureCache::LoadAsync(const std::string_view cache_key, const std::span<const std::byte> encoded)
    -> async::Future<TextureHandle> {
    const std::string key{cache_key};

    // Fast path: a live texture already exists for this key.
    if (renderer_) {
        if (const auto it = cache_.find(key); it != cache_.end()) {
            if (auto existing = it->second.lock()) {
                auto ready = std::make_shared<async::Promise<TextureHandle>>();
                ready->SetValue(TextureHandle{std::move(existing)});
                return ready->GetFuture();
            }
            cache_.erase(it);
        }
    }

    // Concurrent loads of one key share a single in-flight decode.
    if (const auto it = in_flight_.find(key); it != in_flight_.end())
        return it->second->promise->GetFuture();

    auto load = std::make_shared<detail::AsyncTextureLoad>();
    load->key = key;
    load->promise = std::make_shared<async::Promise<TextureHandle>>();

    // Cannot start: resolve immediately with an empty handle (same failure
    // convention as the synchronous Acquire* entry points).
    if (!renderer_ || key.empty() || encoded.empty()) {
        load->promise->SetValue(TextureHandle{});
        return load->promise->GetFuture();
    }

    in_flight_[key] = load;

    // Copy the encoded bytes into the job: the caller's span may point at
    // short-lived storage once LoadAsync returns.
    std::vector<std::byte> owned{encoded.begin(), encoded.end()};
    auto& jobs = atom::job::JobSystem::GetInstance();
    if (!jobs.IsRunning()) {
        load->decoded = image::DecodeImageMemory(owned);
        load->decode_done.store(true, std::memory_order_release);
        return load->promise->GetFuture();
    }

    jobs.Schedule("image:decode", [load, owned = std::move(owned)] {
        auto decoded = image::DecodeImageMemory(owned);
        {
            std::lock_guard lock{load->mutex};
            load->decoded = std::move(decoded);
        }
        load->decode_done.store(true, std::memory_order_release);
    });
    return load->promise->GetFuture();
}

auto TextureCache::LoadAsync(const fs::IFileSystem& filesystem, const fs::AssetPath& path)
    -> async::Future<TextureHandle> {
    const std::string key = path.IsValid() ? std::string{path.String()} : std::string{};

    if (renderer_ && !key.empty()) {
        if (const auto it = cache_.find(key); it != cache_.end()) {
            if (auto existing = it->second.lock()) {
                auto ready = std::make_shared<async::Promise<TextureHandle>>();
                ready->SetValue(TextureHandle{std::move(existing)});
                return ready->GetFuture();
            }
            cache_.erase(it);
        }
    }

    if (!key.empty()) {
        if (const auto it = in_flight_.find(key); it != in_flight_.end())
            return it->second->promise->GetFuture();
    }

    auto load = std::make_shared<detail::AsyncTextureLoad>();
    load->key = key;
    load->promise = std::make_shared<async::Promise<TextureHandle>>();

    if (!renderer_ || key.empty() || !path.IsValid()) {
        load->promise->SetValue(TextureHandle{});
        return load->promise->GetFuture();
    }

    in_flight_[key] = load;

    auto& jobs = atom::job::JobSystem::GetInstance();
    if (!jobs.IsRunning()) {
        std::unique_ptr<fs::IFile> file{};
        std::vector<std::byte> bytes{};
        if (filesystem.OpenRead(path, file) == fs::Result::Success && file &&
            fs::ReadAll(*file, bytes) == fs::Result::Success) {
            load->decoded = image::DecodeImageMemory(bytes);
        }
        load->decode_done.store(true, std::memory_order_release);
        return load->promise->GetFuture();
    }

    jobs.Schedule("image:decode", [load, &filesystem, path] {
        std::unique_ptr<fs::IFile> file{};
        std::vector<std::byte> bytes{};
        image::DecodedImage decoded{};
        if (filesystem.OpenRead(path, file) == fs::Result::Success && file &&
            fs::ReadAll(*file, bytes) == fs::Result::Success) {
            decoded = image::DecodeImageMemory(bytes);
        }
        {
            std::lock_guard lock{load->mutex};
            load->decoded = std::move(decoded);
        }
        load->decode_done.store(true, std::memory_order_release);
    });
    return load->promise->GetFuture();
}

auto TextureCache::PumpAsync() -> void {
    for (auto it = in_flight_.begin(); it != in_flight_.end();) {
        auto& load = *it->second;
        if (!load.decode_done.load(std::memory_order_acquire)) {
            ++it; // still decoding on the worker
            continue;
        }

        atom::image::DecodedImage decoded{};
        {
            std::lock_guard lock{load.mutex};
            decoded = std::move(load.decoded);
        }

        // Materialize on the render thread. A null renderer, an empty key or an
        // invalid decode resolves to an empty handle (failure), matching the
        // synchronous Acquire* convention.
        TextureHandle handle{};
        if (renderer_ && !load.key.empty() && decoded.IsValid())
            handle = AcquireFromDecodedImage(load.key, decoded);
        load.promise->SetValue(std::move(handle));
        it = in_flight_.erase(it);
    }
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

/**
 * @file           : ResourceManager.hpp
 * @brief          : Loader registry and identity-keyed resource cache.
**/

#ifndef ATOM_ASSET_RESOURCE_MANAGER_HPP
#define ATOM_ASSET_RESOURCE_MANAGER_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Asset/IResourceLoader.hpp>
#include <Asset/ResourceHandle.hpp>
#include <Asset/ResourceId.hpp>
#include <Concurrency/Future.hpp>
#include <Error/Error.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom::asset {

// Synchronous resource cache. Same ResourceId returns the same live instance.
// The last handle release destroys the resource unless a recycle callback is
// installed. No LRU eviction in this stage (D5).
//
// Thread model (D6): Acquire is not internally synchronized. Callers must not
// invoke Acquire concurrently on the same manager. Handles may be copied
// across threads only if T itself is safe to share that way.
class ResourceManager final {
    public:
        explicit ResourceManager(const fs::IFileSystem& filesystem);
        ~ResourceManager();

        ResourceManager(const ResourceManager&) = delete;
        auto operator=(const ResourceManager&) = delete;

        // One loader per kind. Registering a second loader for the same kind
        // replaces the previous one only after all live handles using it are
        // already bound; new Acquires use the new loader.
        [[nodiscard]] auto RegisterLoader(std::shared_ptr<IResourceLoader> loader) -> Result<void>;
        [[nodiscard]] auto HasLoader(AssetKind kind) const -> bool;

        // Called when the last handle of a resource is released. Pass an empty
        // callback to destroy immediately.
        auto SetRecycleCallback(ResourceRecycleCallback callback) -> void;

        // The full-fidelity entry point: on failure the error names the code,
        // the operation and the resource path, and the reason a loader gave.
        // This exists because Acquire() below cannot report anything, which made
        // every load failure indistinguishable from an empty cache (CORE-004).
        template <typename T> [[nodiscard]] auto TryAcquire(const ResourceId& id) -> Result<ResourceHandle<T>> {
            auto record = AcquireRecord(id);
            if (!record.has_value())
                return std::unexpected<Error>{std::move(record.error())};

            auto& resolved = *record;
            if (resolved->type != std::type_index(typeid(T))) {
                return std::unexpected<Error>{
                    ToError(AssetResult::TypeMismatch, id, "TryAcquire")
                        .WithDetail(std::string{"requested type does not match the registered loader"})};
            }
            return ResourceHandle<T>{std::move(resolved)};
        }

        // Convenience form: an invalid handle means "could not acquire", and the
        // reason has already been reported through Log (see AcquireRecord), so a
        // failing load is diagnosable instead of silent. Use TryAcquire() when
        // the caller wants to branch on the reason.
        template <typename T> [[nodiscard]] auto Acquire(const ResourceId& id) -> ResourceHandle<T> {
            auto record = AcquireRecord(id);
            if (!record.has_value())
                return ResourceHandle<T>{};
            if ((*record)->type != std::type_index(typeid(T)))
                return ResourceHandle<T>{};
            return ResourceHandle<T>{std::move(*record)};
        }

        // Batch acquire: loads many resources in one call, spreading the
        // expensive per-file work (I/O + decode) across the job system while
        // keeping cache insertion single-threaded. Results are returned in
        // input order; each slot is an independent Result so one failure never
        // masks its neighbours. Falls back to sequential loads when the job
        // system is not running.
        template <typename T> [[nodiscard]] auto TryAcquireBatch(const std::vector<ResourceId>& ids)
            -> std::vector<Result<ResourceHandle<T>>> {
            auto records = AcquireRecords(ids);
            std::vector<Result<ResourceHandle<T>>> results;
            results.reserve(records.size());
            for (auto& record : records) {
                if (!record.has_value()) {
                    results.push_back(std::unexpected<Error>{std::move(record.error())});
                    continue;
                }
                auto& resolved = *record;
                if (resolved->type != std::type_index(typeid(T))) {
                    results.push_back(std::unexpected<Error>{
                        ToError(AssetResult::TypeMismatch, resolved->id, "TryAcquireBatch")
                            .WithDetail(std::string{"requested type does not match the registered loader"})});
                    continue;
                }
                results.push_back(ResourceHandle<T>{std::move(resolved)});
            }
            return results;
        }

        // Convenience batch form mirroring Acquire: an invalid handle means the
        // corresponding slot failed (or type-mismatched), with the reason already
        // reported through Log. Use TryAcquireBatch() to branch on the reason.
        template <typename T> [[nodiscard]] auto AcquireBatch(const std::vector<ResourceId>& ids)
            -> std::vector<ResourceHandle<T>> {
            auto records = AcquireRecords(ids);
            std::vector<ResourceHandle<T>> handles;
            handles.reserve(records.size());
            for (auto& record : records) {
                if (!record.has_value() || (*record)->type != std::type_index(typeid(T))) {
                    handles.emplace_back();
                    continue;
                }
                handles.emplace_back(std::move(*record));
            }
            return handles;
        }

        // --- Async acquire (ARCH-107) ----------------------------------------
        //
        // Returns immediately; the load (I/O + decode) runs on the job system.
        // The handle is finalized on the render thread by PumpAsync(), because
        // cache insertion must stay single-threaded and cannot happen on a
        // worker. Poll Future::IsReady() each frame; once ready, Future::Get()
        // yields the handle (or the failure reason). Concurrent acquires of the
        // same id share one in-flight load; Evict() cancels the in-flight load
        // so an evicted resource is not resurrected.
        template <typename T> [[nodiscard]] auto AcquireAsync(const ResourceId& id) -> async::Future<ResourceHandle<T>> {
            auto promise = std::make_shared<async::Promise<ResourceHandle<T>>>(next_generation_++);

            if (id.IsValid()) {
                const std::string key{id.Key()};
                if (const auto it = cache_.find(key); it != cache_.end()) {
                    if (auto existing = it->second.record.lock()) {
                        if (existing->type != std::type_index(typeid(T)))
                            promise->SetError(ToError(AssetResult::TypeMismatch, existing->id, "AcquireAsync")
                                                  .WithDetail("requested type does not match the registered loader"));
                        else
                            promise->SetValue(ResourceHandle<T>{std::move(existing)});
                        return promise->GetFuture();
                    }
                    cache_.erase(it);
                }
            }

            const auto started = StartAsyncLoad(id);
            if (!started.has_value()) {
                promise->SetError(std::move(started.error())); // already logged in StartAsyncLoad
                return promise->GetFuture();
            }
            const auto async = started.value();

            async->on_success.emplace_back([promise, id](std::shared_ptr<detail::ResourceRecord> record) {
                if (!record) {
                    promise->SetError(ToError(AssetResult::LoadFailed, id, "AcquireAsync"));
                    return;
                }
                if (record->type != std::type_index(typeid(T)))
                    promise->SetError(ToError(AssetResult::TypeMismatch, record->id, "AcquireAsync")
                                          .WithDetail("requested type does not match the registered loader"));
                else
                    promise->SetValue(ResourceHandle<T>{std::move(record)});
            });
            async->on_error.emplace_back([promise](Error error) { promise->SetError(std::move(error)); });
            async->on_cancel.emplace_back([promise]() { promise->Cancel(); });
            return promise->GetFuture();
        }

        template <typename T> [[nodiscard]] auto AcquireAsyncBatch(const std::vector<ResourceId>& ids)
            -> std::vector<async::Future<ResourceHandle<T>>> {
            std::vector<async::Future<ResourceHandle<T>>> futures;
            futures.reserve(ids.size());
            for (const ResourceId& id : ids)
                futures.push_back(AcquireAsync<T>(id));
            return futures;
        }

        // Drains pending async loads: finalizes cache insertion on the owning
        // thread and completes their futures. Render-thread-only; call once per
        // frame outside an active frame (the async counterpart of
        // FlushDeferredDestroys). Without this, futures never become Ready.
        auto PumpAsync() -> void;

        // Drops the cache entry. Live handles keep the resource alive.
        [[nodiscard]] auto Evict(const ResourceId& id) -> bool;
        auto EvictAll() -> void;

        // Number of cache entries that still have at least one live handle.
        [[nodiscard]] auto LiveCount() const -> std::size_t;
        [[nodiscard]] auto CacheCapacity() const -> std::size_t;

    private:
        struct CacheEntry final {
                std::weak_ptr<detail::ResourceRecord> record{};
                std::type_index type{typeid(void)};
        };

        // One in-flight async load, shared by all concurrent acquires of the
        // same id. The worker fills `raw`; the render thread (PumpAsync)
        // finalizes cache insertion and completes every registered consumer.
        struct AsyncLoad final {
                ResourceId id{};
                std::shared_ptr<IResourceLoader> loader{};
                std::uint64_t generation = 0;
                std::mutex mutex;
                std::optional<Result<std::shared_ptr<void>>> raw{};
                std::atomic<bool> cancelled{false};
                std::vector<std::function<void(std::shared_ptr<detail::ResourceRecord>)>> on_success{};
                std::vector<std::function<void(Error)>> on_error{};
                std::vector<std::function<void()>> on_cancel{};
        };

        // Shared core of Acquire/TryAcquire. This is the single place a load can
        // fail and the single place the failure is logged, so one failure
        // produces exactly one diagnostic carrying the resource and the reason.
        [[nodiscard]] auto AcquireRecord(const ResourceId& id)
            -> Result<std::shared_ptr<detail::ResourceRecord>>;

        // Shared core of the batch entry points. Loads in parallel through the
        // job system (one job per unique resource) and inserts into the cache on
        // the calling thread, so cache_ is never mutated concurrently. Results
        // are in input order; duplicate ids share one load.
        [[nodiscard]] auto AcquireRecords(const std::vector<ResourceId>& ids)
            -> std::vector<Result<std::shared_ptr<detail::ResourceRecord>>>;

        // Wraps a loaded resource into a record and caches it. Single-threaded:
        // only ever called on the thread that owns the manager.
        [[nodiscard]] auto InsertRecord(const ResourceId& id, const std::shared_ptr<IResourceLoader>& loader,
                                        std::shared_ptr<void> resource)
            -> std::shared_ptr<detail::ResourceRecord>;

        // Creates (or reuses) an in-flight load and schedules the worker job.
        // Returns an error (already logged) when the load cannot be started.
        [[nodiscard]] auto StartAsyncLoad(const ResourceId& id) -> Result<std::shared_ptr<AsyncLoad>>;
        auto CancelAsyncLoad(AsyncLoad& load) -> void;

        auto SweepExpired() -> void;

        const fs::IFileSystem* filesystem_ = nullptr;
        std::unordered_map<std::string, std::shared_ptr<IResourceLoader>> loaders_{};
        std::unordered_map<std::string, CacheEntry> cache_{};
        std::unordered_map<std::string, std::shared_ptr<AsyncLoad>> in_flight_{};
        std::shared_ptr<detail::RecycleHooks> hooks_{std::make_shared<detail::RecycleHooks>()};
        uint32_t next_generation_ = 1;
};

} // namespace atom::asset

#endif // ATOM_ASSET_RESOURCE_MANAGER_HPP

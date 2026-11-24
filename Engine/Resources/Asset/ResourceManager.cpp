/**
 * @file           : ResourceManager.cpp
 * @brief          : Loader registry and identity-keyed resource cache.
**/

#include "ResourceManager.hpp"

#include <optional>

#include <Concurrency/JobSystem.hpp>
#include <Log/LogSystem.hpp>

namespace atom::asset {
namespace {
// Sweep when the cache holds this many entries. Kept well above typical live
// counts so the amortized cost stays negligible while still bounding growth.
constexpr std::size_t sweep_threshold = 256;

// Loads one resource through its loader. Pure with respect to the manager: it
// only touches the loader and the filesystem, never the cache, so it is safe to
// run from any worker. Returns the failure reason, or nullopt on success (with
// the resource written to `out_resource`).
auto LoadResource(const fs::IFileSystem& filesystem, const ResourceId& id,
                  const std::shared_ptr<IResourceLoader>& loader, std::shared_ptr<void>& out_resource)
    -> std::optional<Error> {
    std::shared_ptr<void> resource{};
    auto result = loader->Load(filesystem, id, resource);
    if (result.has_value()) {
        out_resource = std::move(resource);
        return std::nullopt;
    }
    return std::move(result.error());
}

} // namespace

ResourceManager::ResourceManager(const fs::IFileSystem& filesystem) : filesystem_(&filesystem) {}

ResourceManager::~ResourceManager() {
    // Cancel in-flight loads so pending futures resolve as Cancelled instead of
    // hanging forever. The worker jobs keep their own shared_ptr to the
    // AsyncLoad and finish harmlessly: they only write `raw`, which nothing will
    // ever read again once the manager is gone.
    for (auto& [key, load] : in_flight_)
        CancelAsyncLoad(*load);
    in_flight_.clear();
}

auto ResourceManager::RegisterLoader(std::shared_ptr<IResourceLoader> loader) -> Result<void> {
    if (!loader) {
        return std::unexpected<Error>{ToError(AssetResult::InvalidId, "RegisterLoader")
                                          .WithDetail("null loader")};
    }
    if (loader->Kind() == AssetKind::Unknown) {
        return std::unexpected<Error>{ToError(AssetResult::InvalidId, "RegisterLoader")
                                          .WithDetail("loader does not declare an AssetKind")};
    }
    loaders_[std::string{ToString(loader->Kind())}] = std::move(loader);
    return Ok();
}

auto ResourceManager::HasLoader(const AssetKind kind) const -> bool {
    return loaders_.contains(std::string{ToString(kind)});
}

auto ResourceManager::SetRecycleCallback(ResourceRecycleCallback callback) -> void {
    hooks_->callback = std::move(callback);
}

auto ResourceManager::Evict(const ResourceId& id) -> bool {
    if (!id.IsValid())
        return false;
    const std::string key{id.Key()};
    // Cancel any in-flight load so an evicted resource is not resurrected by a
    // pending acquire that completes after this call.
    if (const auto pending = in_flight_.find(key); pending != in_flight_.end()) {
        CancelAsyncLoad(*pending->second);
        in_flight_.erase(pending);
    }
    const auto it = cache_.find(key);
    if (it == cache_.end())
        return false;
    cache_.erase(it);
    return true;
}

auto ResourceManager::EvictAll() -> void {
    for (auto& [key, load] : in_flight_)
        CancelAsyncLoad(*load);
    in_flight_.clear();
    cache_.clear();
}

auto ResourceManager::LiveCount() const -> std::size_t {
    std::size_t live = 0;
    for (const auto& [key, entry] : cache_) {
        if (!entry.record.expired())
            ++live;
    }
    return live;
}

auto ResourceManager::CacheCapacity() const -> std::size_t {
    return cache_.size();
}

auto ResourceManager::SweepExpired() -> void {
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.record.expired())
            it = cache_.erase(it);
        else
            ++it;
    }
}

auto ResourceManager::AcquireRecord(const ResourceId& id)
    -> Result<std::shared_ptr<detail::ResourceRecord>> {
    // Every failure below is reported through the returned Error and logged
    // here, at the site that knows the operation and the resource. Upper layers
    // therefore do not log again: one failure, one diagnostic (CORE-004).
    const auto fail = [](Error error) -> Result<std::shared_ptr<detail::ResourceRecord>> {
        LOG_ERROR(atom::log::asset::Resource, "ResourceManager: " + error.Message());
        return std::unexpected<Error>{std::move(error)};
    };

    if (!id.IsValid()) {
        return fail(ToError(AssetResult::InvalidId, id, "AcquireRecord")
                        .WithDetail("resource id is not valid"));
    }
    if (filesystem_ == nullptr) {
        return fail(ToError(AssetResult::InvalidId, id, "AcquireRecord").WithDetail("no filesystem bound"));
    }

    const std::string key{id.Key()};
    if (const auto it = cache_.find(key); it != cache_.end()) {
        if (auto existing = it->second.record.lock()) {
            return std::shared_ptr<detail::ResourceRecord>{std::move(existing)};
        }
        cache_.erase(it);
    }

    const auto loader = loaders_.find(std::string{ToString(id.Kind())});
    if (loader == loaders_.end() || !loader->second)
        return fail(ToError(AssetResult::NoLoader, id, "AcquireRecord"));

    std::shared_ptr<void> resource{};
    auto loaded = loader->second->Load(*filesystem_, id, resource);
    if (!loaded.has_value()) {
        // The loader's own error is more specific than anything this layer could
        // invent (which decoder, which format, which byte), so it is forwarded
        // with the operation that was attempted on top.
        auto error = std::move(loaded.error());
        error.WithOperation("AcquireRecord");
        error.WithResource(id.Path().String());
        return fail(std::move(error));
    }
    if (!resource) {
        return fail(ToError(AssetResult::LoadFailed, id, "AcquireRecord")
                        .WithDetail("loader reported success without a resource"));
    }

    return InsertRecord(id, loader->second, std::move(resource));
}

auto ResourceManager::InsertRecord(const ResourceId& id, const std::shared_ptr<IResourceLoader>& loader,
                                   std::shared_ptr<void> resource)
    -> std::shared_ptr<detail::ResourceRecord> {
    auto record = std::make_shared<detail::ResourceRecord>();
    record->id = id;
    record->generation = next_generation_++;
    record->resource = std::move(resource);
    record->type = loader->ResourceType();
    record->hooks = hooks_;

    const std::string key{id.Key()};
    cache_[key] = CacheEntry{record, record->type};
    // Lazily reap expired entries instead of rescanning the whole cache on
    // every successful load. A stale entry is harmless: Acquire replaces it on
    // the next miss for the same key, so sweeping only guards against unbounded
    // growth of dead entries.
    if (cache_.size() >= sweep_threshold)
        SweepExpired();
    return record;
}

auto ResourceManager::StartAsyncLoad(const ResourceId& id) -> Result<std::shared_ptr<AsyncLoad>> {
    // Same single-diagnostic contract as AcquireRecord: every failure is logged
    // here (the site that knows the operation and resource) and returned so the
    // caller can attach it to its Future. Upper layers never log again.
    const auto fail = [](Error error) -> Result<std::shared_ptr<AsyncLoad>> {
        LOG_ERROR(atom::log::asset::Resource, "ResourceManager: " + error.Message());
        return std::unexpected<Error>{std::move(error)};
    };

    if (!id.IsValid())
        return fail(ToError(AssetResult::InvalidId, id, "StartAsyncLoad")
                        .WithDetail("resource id is not valid"));
    if (filesystem_ == nullptr)
        return fail(ToError(AssetResult::InvalidId, id, "StartAsyncLoad").WithDetail("no filesystem bound"));

    const std::string key{id.Key()};

    // Concurrent acquires of the same id share one in-flight load: the second
    // caller registers its callbacks on the existing AsyncLoad instead of
    // starting a duplicate job. Only the render thread reaches here, so no lock
    // is needed on in_flight_ itself.
    if (const auto it = in_flight_.find(key); it != in_flight_.end())
        return it->second;

    const auto loader = loaders_.find(std::string{ToString(id.Kind())});
    if (loader == loaders_.end() || !loader->second)
        return fail(ToError(AssetResult::NoLoader, id, "StartAsyncLoad"));

    auto async = std::make_shared<AsyncLoad>();
    async->id = id;
    async->loader = loader->second;
    async->generation = next_generation_++;

    // The worker touches only the loader, the filesystem and the AsyncLoad box,
    // never the cache or the manager, so it is safe off-thread. `filesystem` is
    // captured by pointer (not `this`) so a worker that outlives the manager
    // never dereferences freed manager state; the filesystem itself is owned by
    // the engine and outlives the manager.
    const fs::IFileSystem* filesystem = filesystem_;
    const std::shared_ptr<IResourceLoader> load_loader = loader->second;
    auto work = [filesystem, id, load_loader, async] {
        std::shared_ptr<void> resource{};
        const auto load_error = LoadResource(*filesystem, id, load_loader, resource);
        Result<std::shared_ptr<void>> outcome =
            load_error ? Result<std::shared_ptr<void>>{std::unexpected<Error>{std::move(*load_error)}}
                       : Result<std::shared_ptr<void>>{std::move(resource)};
        {
            std::lock_guard lock{async->mutex};
            if (!async->cancelled.load(std::memory_order_acquire))
                async->raw = std::move(outcome);
        }
    };

    auto& jobs = atom::job::JobSystem::GetInstance();
    if (jobs.IsRunning())
        jobs.Schedule("asset:async-load", std::move(work));
    else
        work(); // no scheduler running: decode inline, PumpAsync still finalizes

    in_flight_[key] = async;
    return async;
}

auto ResourceManager::CancelAsyncLoad(AsyncLoad& load) -> void {
    // exchange() returns the previous flag, so only the first cancel fires the
    // callbacks: a double cancel still completes each consumer future once.
    if (load.cancelled.exchange(true))
        return;
    for (const auto& callback : load.on_cancel)
        callback();
}

auto ResourceManager::PumpAsync() -> void {
    for (auto it = in_flight_.begin(); it != in_flight_.end();) {
        AsyncLoad& load = *it->second;

        if (load.cancelled.load(std::memory_order_acquire)) {
            for (const auto& callback : load.on_cancel)
                callback();
            it = in_flight_.erase(it);
            continue;
        }

        std::optional<Result<std::shared_ptr<void>>> raw;
        {
            std::lock_guard lock{load.mutex};
            raw = std::move(load.raw);
        }
        if (!raw.has_value()) {
            ++it; // still decoding on the worker
            continue;
        }

        // Terminal: cache insertion stays single-threaded on the render thread.
        // Each failure is logged exactly once (CORE-004); every consumer future
        // for this id is then completed with the same outcome.
        auto& outcome = *raw;
        if (!outcome.has_value()) {
            auto error = std::move(outcome.error());
            error.WithOperation("PumpAsync");
            error.WithResource(load.id.Path().String());
            LOG_ERROR(atom::log::asset::Resource, "ResourceManager: " + error.Message());
            for (const auto& callback : load.on_error)
                callback(error);
        } else if (!outcome.value()) {
            const auto error = ToError(AssetResult::LoadFailed, load.id, "PumpAsync")
                                   .WithDetail("loader reported success without a resource");
            LOG_ERROR(atom::log::asset::Resource, "ResourceManager: " + error.Message());
            for (const auto& callback : load.on_error)
                callback(error);
        } else {
            const auto record = InsertRecord(load.id, load.loader, std::move(outcome.value()));
            for (const auto& callback : load.on_success)
                callback(record);
        }
        it = in_flight_.erase(it);
    }
}

auto ResourceManager::AcquireRecords(const std::vector<ResourceId>& ids)
    -> std::vector<Result<std::shared_ptr<detail::ResourceRecord>>> {
    std::vector<Result<std::shared_ptr<detail::ResourceRecord>>> results(ids.size());

    constexpr std::size_t kNoSlot = static_cast<std::size_t>(-1);

    // --- Phase A (single-threaded): resolve the cache, validate, and collect
    //     the ids that actually need loading. Duplicate ids (same key) share a
    //     single pending slot, so a resource is never loaded twice in one batch.
    std::vector<std::size_t> slot_for(ids.size(), kNoSlot);
    std::vector<ResourceId> pending_ids;
    std::vector<std::shared_ptr<IResourceLoader>> pending_loaders;
    std::unordered_map<std::string, std::size_t> slot_by_key;

    for (std::size_t i = 0; i < ids.size(); ++i) {
        const ResourceId& id = ids[i];
        if (!id.IsValid()) {
            results[i] = std::unexpected<Error>{ToError(AssetResult::InvalidId, id, "AcquireRecords")
                                                    .WithDetail("resource id is not valid")};
            continue;
        }
        if (filesystem_ == nullptr) {
            results[i] = std::unexpected<Error>{ToError(AssetResult::InvalidId, id, "AcquireRecords")
                                                    .WithDetail("no filesystem bound")};
            continue;
        }

        const std::string key{id.Key()};
        if (const auto it = cache_.find(key); it != cache_.end()) {
            if (auto existing = it->second.record.lock()) {
                results[i] = std::shared_ptr<detail::ResourceRecord>{std::move(existing)};
                continue;
            }
            cache_.erase(it);
        }

        if (const auto it = slot_by_key.find(key); it != slot_by_key.end()) {
            slot_for[i] = it->second; // same key: reuse the pending slot
            continue;
        }

        const auto loader = loaders_.find(std::string{ToString(id.Kind())});
        if (loader == loaders_.end() || !loader->second) {
            results[i] = std::unexpected<Error>{ToError(AssetResult::NoLoader, id, "AcquireRecords")};
            continue;
        }

        const std::size_t slot = pending_ids.size();
        pending_ids.push_back(id);
        pending_loaders.push_back(loader->second);
        slot_by_key.emplace(key, slot);
        slot_for[i] = slot;
    }

    // --- Phase B (parallel): load each unique resource. Each job only reads
    //     its own loader/filesystem and writes its own result slot, so there is
    //     no shared mutable state between workers.
    const std::size_t pending_count = pending_ids.size();
    std::vector<std::shared_ptr<void>> loaded(pending_count);
    std::vector<std::optional<Error>> load_errors(pending_count);

    if (pending_count > 0) {
        auto& jobs = atom::job::JobSystem::GetInstance();
        if (jobs.IsRunning()) {
            std::vector<atom::job::JobHandle> handles;
            handles.reserve(pending_count);
            for (std::size_t k = 0; k < pending_count; ++k) {
                const ResourceId id = pending_ids[k];
                const std::shared_ptr<IResourceLoader> loader = pending_loaders[k];
                handles.push_back(jobs.Schedule("asset:load", [this, k, id, loader, &loaded, &load_errors] {
                    load_errors[k] = LoadResource(*filesystem_, id, loader, loaded[k]);
                }));
            }
            jobs.WaitAll(handles);
        } else {
            for (std::size_t k = 0; k < pending_count; ++k)
                load_errors[k] = LoadResource(*filesystem_, pending_ids[k], pending_loaders[k], loaded[k]);
        }
    }

    // --- Phase C (single-threaded): insert into the cache and fill results.
    //     Every failure is logged exactly once (at the slot), and duplicate ids
    //     share the same record/error.
    std::vector<std::shared_ptr<detail::ResourceRecord>> slot_records(pending_count);
    std::vector<std::optional<Error>> slot_errors(pending_count);
    for (std::size_t k = 0; k < pending_count; ++k) {
        if (load_errors[k]) {
            auto error = std::move(*load_errors[k]);
            error.WithOperation("AcquireRecords");
            error.WithResource(pending_ids[k].Path().String());
            LOG_ERROR(atom::log::asset::Resource, "ResourceManager: " + error.Message());
            slot_errors[k] = std::move(error);
        } else if (!loaded[k]) {
            auto error = ToError(AssetResult::LoadFailed, pending_ids[k], "AcquireRecords")
                             .WithDetail("loader reported success without a resource");
            LOG_ERROR(atom::log::asset::Resource, "ResourceManager: " + error.Message());
            slot_errors[k] = std::move(error);
        } else {
            slot_records[k] = InsertRecord(pending_ids[k], pending_loaders[k], std::move(loaded[k]));
        }
    }

    for (std::size_t i = 0; i < ids.size(); ++i) {
        const std::size_t slot = slot_for[i];
        if (slot == kNoSlot)
            continue; // already resolved in phase A
        if (slot_errors[slot]) {
            results[i] = std::unexpected<Error>{*slot_errors[slot]}; // copy: shared by duplicate ids
        } else {
            results[i] = slot_records[slot];
        }
    }

    return results;
}

} // namespace atom::asset

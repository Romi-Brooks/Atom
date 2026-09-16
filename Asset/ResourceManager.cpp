/**
 * @file           : ResourceManager.cpp
 * @brief          : Loader registry and identity-keyed resource cache.
**/

#include "ResourceManager.hpp"

namespace atom::asset {

ResourceManager::ResourceManager(const fs::IFileSystem& filesystem) : filesystem_(&filesystem) {}

ResourceManager::~ResourceManager() = default;

auto ResourceManager::RegisterLoader(std::shared_ptr<IResourceLoader> loader) -> AssetResult {
    if (!loader)
        return AssetResult::InvalidId;
    if (loader->Kind() == AssetKind::Unknown)
        return AssetResult::InvalidId;
    loaders_[std::string{ToString(loader->Kind())}] = std::move(loader);
    return AssetResult::Success;
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
    const auto it = cache_.find(std::string{id.Key()});
    if (it == cache_.end())
        return false;
    cache_.erase(it);
    return true;
}

auto ResourceManager::EvictAll() -> void {
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

auto ResourceManager::AcquireRecord(const ResourceId& id, std::shared_ptr<detail::ResourceRecord>& output)
    -> AssetResult {
    output.reset();
    if (!id.IsValid() || !filesystem_)
        return AssetResult::InvalidId;

    const std::string key{id.Key()};
    if (const auto it = cache_.find(key); it != cache_.end()) {
        if (auto existing = it->second.record.lock()) {
            output = std::move(existing);
            return AssetResult::Success;
        }
        cache_.erase(it);
    }

    const auto loader = loaders_.find(std::string{ToString(id.Kind())});
    if (loader == loaders_.end() || !loader->second)
        return AssetResult::NoLoader;

    std::shared_ptr<void> resource{};
    const AssetResult loaded = loader->second->Load(*filesystem_, id, resource);
    if (loaded != AssetResult::Success || !resource)
        return loaded == AssetResult::Success ? AssetResult::LoadFailed : loaded;

    auto record = std::make_shared<detail::ResourceRecord>();
    record->id = id;
    record->generation = next_generation_++;
    record->resource = std::move(resource);
    record->type = loader->second->ResourceType();
    record->hooks = hooks_;

    cache_[key] = CacheEntry{record, record->type};
    SweepExpired();
    output = std::move(record);
    return AssetResult::Success;
}

} // namespace atom::asset

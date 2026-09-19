/**
 * @file           : ResourceManager.hpp
 * @brief          : Loader registry and identity-keyed resource cache.
**/

#ifndef ATOM_ASSET_RESOURCE_MANAGER_HPP
#define ATOM_ASSET_RESOURCE_MANAGER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

#include <Asset/IResourceLoader.hpp>
#include <Asset/ResourceHandle.hpp>
#include <Asset/ResourceId.hpp>
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
        ResourceManager& operator=(const ResourceManager&) = delete;

        // One loader per kind. Registering a second loader for the same kind
        // replaces the previous one only after all live handles using it are
        // already bound; new Acquires use the new loader.
        auto RegisterLoader(std::shared_ptr<IResourceLoader> loader) -> AssetResult;
        [[nodiscard]] auto HasLoader(AssetKind kind) const -> bool;

        // Called when the last handle of a resource is released. Pass an empty
        // callback to destroy immediately.
        auto SetRecycleCallback(ResourceRecycleCallback callback) -> void;

        template <typename T> [[nodiscard]] auto Acquire(const ResourceId& id) -> ResourceHandle<T> {
            std::shared_ptr<detail::ResourceRecord> record{};
            const AssetResult result = AcquireRecord(id, record);
            if (result != AssetResult::Success || !record)
                return ResourceHandle<T>{};
            if (record->type != std::type_index(typeid(T)))
                return ResourceHandle<T>{};
            return ResourceHandle<T>{std::move(record)};
        }

        // Drops the cache entry. Live handles keep the resource alive.
        auto Evict(const ResourceId& id) -> bool;
        auto EvictAll() -> void;

        // Number of cache entries that still have at least one live handle.
        [[nodiscard]] auto LiveCount() const -> std::size_t;
        [[nodiscard]] auto CacheCapacity() const -> std::size_t;

    private:
        struct CacheEntry final {
                std::weak_ptr<detail::ResourceRecord> record{};
                std::type_index type{typeid(void)};
        };

        auto AcquireRecord(const ResourceId& id, std::shared_ptr<detail::ResourceRecord>& output) -> AssetResult;
        auto SweepExpired() -> void;

        const fs::IFileSystem* filesystem_ = nullptr;
        std::unordered_map<std::string, std::shared_ptr<IResourceLoader>> loaders_{};
        std::unordered_map<std::string, CacheEntry> cache_{};
        std::shared_ptr<detail::RecycleHooks> hooks_{std::make_shared<detail::RecycleHooks>()};
        uint32_t next_generation_ = 1;
};

} // namespace atom::asset

#endif // ATOM_ASSET_RESOURCE_MANAGER_HPP

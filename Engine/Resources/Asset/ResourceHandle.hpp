/**
 * @file           : ResourceHandle.hpp
 * @brief          : Shared, generation-tagged handle to a cached resource.
**/

#ifndef ATOM_ASSET_RESOURCE_HANDLE_HPP
#define ATOM_ASSET_RESOURCE_HANDLE_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <utility>

#include <Asset/ResourceId.hpp>

namespace atom::asset {

// Invoked when the last handle for a resource is released. The callback owns
// the resource shared_ptr for the duration of the call and may move it into a
// deferred-destroy queue (GPU frame-boundary reclaim). If no callback is set,
// the resource is destroyed when the last handle dies.
using ResourceRecycleCallback =
    std::function<void(const ResourceId& id, uint32_t generation, std::shared_ptr<void> resource)>;

namespace detail {

// Shared so ResourceManager::SetRecycleCallback applies to already-cached
// records; destruction always observes the manager's current hook.
struct RecycleHooks final {
        ResourceRecycleCallback callback{};
};

struct ResourceRecord final {
        ResourceId id{};
        uint32_t generation = 0;
        std::shared_ptr<void> resource{};
        std::type_index type{typeid(void)};
        std::shared_ptr<RecycleHooks> hooks{};

        ResourceRecord() = default;
        ~ResourceRecord() {
            if (hooks && hooks->callback && resource)
                hooks->callback(id, generation, std::move(resource));
        }

        ResourceRecord(const ResourceRecord&) = delete;
        ResourceRecord& operator=(const ResourceRecord&) = delete;
};

} // namespace detail

// Copyable shared handle. Destructor only drops the last reference; actual
// teardown is the record destructor (or a recycle callback registered on the
// manager). Handles stay valid across cache eviction: they pin the record they
// were created from.
template <typename T> class ResourceHandle final {
    public:
        ResourceHandle() = default;
        explicit ResourceHandle(std::shared_ptr<detail::ResourceRecord> record) : record_(std::move(record)) {}

        [[nodiscard]] auto IsValid() const -> bool {
            return record_ && record_->resource && record_->type == std::type_index(typeid(T));
        }

        [[nodiscard]] auto Get() const -> T* {
            if (!IsValid())
                return nullptr;
            return static_cast<T*>(record_->resource.get());
        }

        [[nodiscard]] auto operator->() const -> T* {
            return Get();
        }

        [[nodiscard]] auto operator*() const -> T& {
            return *Get();
        }

        [[nodiscard]] auto Id() const -> const ResourceId* {
            return record_ ? &record_->id : nullptr;
        }

        [[nodiscard]] auto Generation() const -> uint32_t {
            return record_ ? record_->generation : 0;
        }

        [[nodiscard]] auto UseCount() const -> long {
            return record_ ? record_.use_count() : 0;
        }

        explicit operator bool() const {
            return IsValid();
        }

    private:
        std::shared_ptr<detail::ResourceRecord> record_{};
};

} // namespace atom::asset

#endif // ATOM_ASSET_RESOURCE_HANDLE_HPP

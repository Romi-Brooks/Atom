/**
 * @file           : IResourceLoader.hpp
 * @brief          : Loader contract for turning an AssetPath into a resource.
**/

#ifndef ATOM_ASSET_IRESOURCE_LOADER_HPP
#define ATOM_ASSET_IRESOURCE_LOADER_HPP

#include <cstdint>
#include <memory>
#include <typeindex>

#include <Asset/AssetKind.hpp>
#include <Asset/ResourceId.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom::asset {

enum class AssetResult : uint8_t {
    Success,
    InvalidId,
    NoLoader,
    NotFound,
    NotFile,
    IoError,
    TypeMismatch,
    LoadFailed,
};

class IResourceLoader {
    public:
        virtual ~IResourceLoader() = default;

        [[nodiscard]] virtual auto Kind() const -> AssetKind = 0;
        [[nodiscard]] virtual auto ResourceType() const -> std::type_index = 0;

        // Must produce a shared_ptr whose type matches ResourceType().
        // Loaders must not assume a native disk path.
        virtual auto Load(const fs::IFileSystem& filesystem, const ResourceId& id, std::shared_ptr<void>& output)
            -> AssetResult = 0;
};

// Convenience base that converts the type-erased Load entry point into a
// typed LoadTyped implementation.
template <typename T> class TypedResourceLoader : public IResourceLoader {
    public:
        [[nodiscard]] auto ResourceType() const -> std::type_index override {
            return typeid(T);
        }

        auto Load(const fs::IFileSystem& filesystem, const ResourceId& id, std::shared_ptr<void>& output)
            -> AssetResult override {
            std::shared_ptr<T> typed{};
            const AssetResult result = LoadTyped(filesystem, id, typed);
            if (result != AssetResult::Success || !typed) {
                output.reset();
                return result == AssetResult::Success ? AssetResult::LoadFailed : result;
            }
            output = std::move(typed);
            return AssetResult::Success;
        }

    protected:
        virtual auto LoadTyped(const fs::IFileSystem& filesystem, const ResourceId& id, std::shared_ptr<T>& output)
            -> AssetResult = 0;
};

[[nodiscard]] auto MapFsResult(fs::Result result) -> AssetResult;

} // namespace atom::asset

#endif // ATOM_ASSET_IRESOURCE_LOADER_HPP

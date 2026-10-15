/**
 * @file           : IResourceLoader.hpp
 * @brief          : Loader contract for turning an AssetPath into a resource.
**/

#ifndef ATOM_ASSET_IRESOURCE_LOADER_HPP
#define ATOM_ASSET_IRESOURCE_LOADER_HPP

#include <cstdint>
#include <memory>
#include <string_view>
#include <typeindex>

#include <Asset/AssetKind.hpp>
#include <Asset/ResourceId.hpp>
#include <Error/Error.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom::asset {

// Failure codes a loader may raise. Loaders are not limited to these: they can
// return any ErrorCode from the Asset block (for example the decoder-specific
// codes), which is why Load() returns a unified Error rather than this enum.
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

[[nodiscard]] constexpr auto AssetResultName(AssetResult result) -> std::string_view {
    switch (result) {
        case AssetResult::Success:
            return "Success";
        case AssetResult::InvalidId:
            return "InvalidId";
        case AssetResult::NoLoader:
            return "NoLoader";
        case AssetResult::NotFound:
            return "NotFound";
        case AssetResult::NotFile:
            return "NotFile";
        case AssetResult::IoError:
            return "IoError";
        case AssetResult::TypeMismatch:
            return "TypeMismatch";
        case AssetResult::LoadFailed:
            return "LoadFailed";
    }
    return "UnknownAssetResult";
}

// Translates an asset-layer code into the unified error value. `id` is used for
// the resource path, so failures carry the identity they were applied to.
[[nodiscard]] auto ToError(AssetResult result, const ResourceId& id, std::string_view operation = {}) -> Error;
[[nodiscard]] auto ToError(AssetResult result, std::string_view operation = {}) -> Error;

// Maps a filesystem code straight into the unified error value, so a loader can
// forward VFS failures without passing through the AssetResult enum:
//     const auto opened = filesystem.OpenRead(id.Path(), file);
//     if (opened != fs::Result::Success)
//         return std::unexpected<Error>{fs::ToError(opened, "OpenRead").WithResource(id.Path().String())};
[[nodiscard]] auto MapFsResult(fs::Result result) -> AssetResult;

class IResourceLoader {
    public:
        virtual ~IResourceLoader() = default;

        [[nodiscard]] virtual auto Kind() const -> AssetKind = 0;
        [[nodiscard]] virtual auto ResourceType() const -> std::type_index = 0;

        // Produces a shared_ptr whose type matches ResourceType(). Loaders must
        // not assume a native disk path. On failure the error identifies the
        // resource and, where the loader knows it, the underlying reason.
        [[nodiscard]] virtual auto Load(const fs::IFileSystem& filesystem, const ResourceId& id,
                                        std::shared_ptr<void>& output) -> Result<void> = 0;
};

// Convenience base that converts the type-erased Load entry point into a typed
// LoadTyped implementation.
template <typename T> class TypedResourceLoader : public IResourceLoader {
    public:
        [[nodiscard]] auto ResourceType() const -> std::type_index override {
            return typeid(T);
        }

        [[nodiscard]] auto Load(const fs::IFileSystem& filesystem, const ResourceId& id,
                                std::shared_ptr<void>& output) -> Result<void> override {
            output.reset();
            std::shared_ptr<T> typed{};
            auto loaded = LoadTyped(filesystem, id, typed);
            if (!loaded.has_value())
                return loaded;
            if (!typed) {
                // A loader that reports success without producing a resource is
                // a loader bug, not an I/O failure: name it as such instead of
                // letting a null resource travel upwards.
                return std::unexpected<Error>{ToError(AssetResult::LoadFailed, id, "LoadTyped")
                                                  .WithDetail("loader reported success without a resource")};
            }
            output = std::move(typed);
            return Ok();
        }

    protected:
        [[nodiscard]] virtual auto LoadTyped(const fs::IFileSystem& filesystem, const ResourceId& id,
                                             std::shared_ptr<T>& output) -> Result<void> = 0;
};

} // namespace atom::asset

#endif // ATOM_ASSET_IRESOURCE_LOADER_HPP

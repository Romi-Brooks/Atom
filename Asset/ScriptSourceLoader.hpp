/**
 * @file           : ScriptSourceLoader.hpp
 * @brief          : Loads Lua script source text as a shareable resource.
**/

#ifndef ATOM_ASSET_SCRIPT_SOURCE_LOADER_HPP
#define ATOM_ASSET_SCRIPT_SOURCE_LOADER_HPP

#include <memory>
#include <string>

#include <Asset/IResourceLoader.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom {

// Resource payload is the raw script source. Execution stays with LuaLoader so
// multiple Lua states can share one cached source string.
class ScriptSourceLoader final : public asset::TypedResourceLoader<std::string> {
    public:
        [[nodiscard]] auto Kind() const -> asset::AssetKind override {
            return asset::AssetKind::Script;
        }

    protected:
        auto LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                       std::shared_ptr<std::string>& output) -> asset::AssetResult override;
};

} // namespace atom

#endif // ATOM_ASSET_SCRIPT_SOURCE_LOADER_HPP

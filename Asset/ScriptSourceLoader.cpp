/**
 * @file           : ScriptSourceLoader.cpp
 * @brief          : Loads Lua script source text from an IFileSystem.
**/

#include "ScriptSourceLoader.hpp"

#include <vector>

namespace atom {

auto ScriptSourceLoader::LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                                   std::shared_ptr<std::string>& output) -> asset::AssetResult {
    std::unique_ptr<fs::IFile> file{};
    const fs::Result opened = filesystem.OpenRead(id.Path(), file);
    if (opened != fs::Result::Success)
        return asset::MapFsResult(opened);

    std::vector<std::byte> bytes{};
    if (fs::ReadAll(*file, bytes) != fs::Result::Success)
        return asset::AssetResult::LoadFailed;

    output = std::make_shared<std::string>(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return asset::AssetResult::Success;
}

} // namespace atom

/**
 * @file           : ScriptSourceLoader.cpp
 * @brief          : Loads Lua script source text from an IFileSystem.
**/

#include "ScriptSourceLoader.hpp"

#include <vector>

namespace atom {

auto ScriptSourceLoader::LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                                   std::shared_ptr<std::string>& output) -> Result<void> {
    std::unique_ptr<fs::IFile> file{};
    const fs::Result opened = filesystem.OpenRead(id.Path(), file);
    if (opened != fs::Result::Success) {
        // Report the reason the VFS gave, tagged with the resource, instead of
        // collapsing it into a generic load failure.
        return std::unexpected<Error>{
            fs::ToError(opened, "OpenRead", id.Path().String())};
    }

    std::vector<std::byte> bytes{};
    const fs::Result read = fs::ReadAll(*file, bytes);
    if (read != fs::Result::Success)
        return std::unexpected<Error>{fs::ToError(read, "ReadAll", id.Path().String())};

    output = std::make_shared<std::string>(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return Ok();
}

} // namespace atom

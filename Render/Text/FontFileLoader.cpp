/**
 * @file           : FontFileLoader.cpp
 * @brief          : FontFileLoader implementation.
**/

#include "FontFileLoader.hpp"

#include <utility>
#include <vector>

namespace atom::render {

auto FontFileLoader::LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                               std::shared_ptr<FontData>& output) -> Result<void> {
    std::unique_ptr<fs::IFile> file{};
    const fs::Result opened = filesystem.OpenRead(id.Path(), file);
    if (opened != fs::Result::Success)
        return std::unexpected<Error>{fs::ToError(opened, "OpenRead", id.Path().String())};

    std::vector<std::byte> bytes{};
    const fs::Result read = fs::ReadAll(*file, bytes);
    if (read != fs::Result::Success)
        return std::unexpected<Error>{fs::ToError(read, "ReadAll", id.Path().String())};

    if (bytes.empty()) {
        return std::unexpected<Error>{Error{ErrorCode::AssetLoadFailed, Module::Asset, "FontFileLoader"}
                                          .WithResource(id.Path().String())
                                          .WithDetail("font file is empty")};
    }

    auto data = std::make_shared<FontData>();
    data->bytes = std::move(bytes);
    output = std::move(data);
    return Ok();
}

} // namespace atom::render

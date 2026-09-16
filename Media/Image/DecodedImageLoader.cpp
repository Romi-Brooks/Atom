/**
 * @file           : DecodedImageLoader.cpp
 * @brief          : DecodedImageLoader implementation.
**/

#include "DecodedImageLoader.hpp"

#include <vector>

namespace atom::image {

auto DecodedImageLoader::LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                                   std::shared_ptr<DecodedImage>& output) -> asset::AssetResult {
    std::unique_ptr<fs::IFile> file{};
    const fs::Result opened = filesystem.OpenRead(id.Path(), file);
    if (opened != fs::Result::Success)
        return asset::MapFsResult(opened);

    std::vector<std::byte> bytes{};
    if (fs::ReadAll(*file, bytes) != fs::Result::Success)
        return asset::AssetResult::LoadFailed;

    auto decoded = std::make_shared<DecodedImage>(DecodeImageMemory(bytes));
    if (!decoded->IsValid())
        return asset::AssetResult::LoadFailed;
    output = std::move(decoded);
    return asset::AssetResult::Success;
}

} // namespace atom::image

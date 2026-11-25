/**
 * @file           : DecodedImageLoader.cpp
 * @brief          : DecodedImageLoader implementation.
**/

#include "DecodedImageLoader.hpp"

#include <vector>

namespace atom::image {

auto DecodedImageLoader::LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                                   std::shared_ptr<DecodedImage>& output) -> Result<void> {
    std::unique_ptr<fs::IFile> file{};
    const fs::Result opened = filesystem.OpenRead(id.Path(), file);
    if (opened != fs::Result::Success)
        return std::unexpected<Error>{fs::ToError(opened, "OpenRead", id.Path().String())};

    std::vector<std::byte> bytes{};
    const fs::Result read = fs::ReadAll(*file, bytes);
    if (read != fs::Result::Success)
        return std::unexpected<Error>{fs::ToError(read, "ReadAll", id.Path().String())};

    auto decoded = std::make_shared<DecodedImage>(DecodeImageMemory(bytes));
    if (!decoded->IsValid()) {
        // The decoder only reports "invalid" here; the size is the one fact this
        // layer can add, and it is what distinguishes a corrupt file from an
        // empty one during triage.
        return std::unexpected<Error>{
            Error{ErrorCode::AssetImageDecodeFailed, Module::Asset, "DecodeImageMemory"}
                .WithResource(id.Path().String())
                .WithDetail(std::to_string(bytes.size()) + " byte(s)")};
    }
    output = std::move(decoded);
    return Ok();
}

} // namespace atom::image

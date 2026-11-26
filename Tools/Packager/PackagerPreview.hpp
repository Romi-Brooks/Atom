#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Atom/Resources/Filesystem/PackageFileSystem.hpp>
#include <Atom/Resources/Media/Image/ImageDecoder.hpp>

namespace atom::packager {

struct AssetPreview {
    std::string status;
    std::vector<std::string> lines;
    image::DecodedImage image;
    std::shared_ptr<fs::PackageFileSystem> audio;
};

auto ReadPreview(const std::string& archive, const fs::PackagedAssetInfo& asset) -> AssetPreview;

}

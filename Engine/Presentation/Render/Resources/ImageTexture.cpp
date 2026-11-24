/**
 * @file           : ImageTexture.cpp
 * @brief          : Image decoder to Renderer2D texture bridge implementation.
**/

#include "ImageTexture.hpp"

#include <vector>

#include <Filesystem/Vfs.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

namespace atom::render::resources {

auto CreateTexture(Renderer2D& renderer, const image::DecodedImage& image) -> Renderer2D::Texture* {
    if (!image.IsValid())
        return nullptr;
    return renderer.CreateTexture(image.width, image.height, image.rgba.data());
}

auto LoadTextureMemory(Renderer2D& renderer, const std::span<const std::byte> encoded_image) -> Renderer2D::Texture* {
    return CreateTexture(renderer, image::DecodeImageMemory(encoded_image));
}

auto LoadTextureFileSystem(Renderer2D& renderer, const fs::IFileSystem& filesystem, const fs::AssetPath& path)
    -> Renderer2D::Texture* {
    std::unique_ptr<fs::IFile> file{};
    if (filesystem.OpenRead(path, file) != fs::Result::Success || !file)
        return nullptr;
    std::vector<std::byte> bytes{};
    if (fs::ReadAll(*file, bytes) != fs::Result::Success)
        return nullptr;
    return LoadTextureMemory(renderer, bytes);
}

auto LoadTexture(Renderer2D& renderer, const std::string& path) -> Renderer2D::Texture* {
    fs::AssetPath asset_path{};
    if (!fs::AssetPath::TryParse(path, asset_path))
        return nullptr;
    return LoadTextureFileSystem(renderer, fs::Vfs::GetInstance(), asset_path);
}

} // namespace atom::render::resources

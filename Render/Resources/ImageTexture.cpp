/**
 * @file           : ImageTexture.cpp
 * @brief          : Image decoder to Renderer2D texture bridge implementation.
**/

#include "ImageTexture.hpp"

#include <Render/Renderer2D/Renderer2D.hpp>

namespace atom::render::resources {

auto CreateTexture(Renderer2D& renderer, const image::DecodedImage& image) -> Renderer2D::Texture* {
    if (!image.IsValid())
        return nullptr;
    return renderer.CreateTexture(image.width, image.height, image.rgba.data());
}

auto LoadTextureFile(Renderer2D& renderer, const std::string& path) -> Renderer2D::Texture* {
    return CreateTexture(renderer, image::DecodeImageFile(path));
}

auto LoadTextureMemory(Renderer2D& renderer, const std::span<const std::byte> encoded_image) -> Renderer2D::Texture* {
    return CreateTexture(renderer, image::DecodeImageMemory(encoded_image));
}

} // namespace atom::render::resources

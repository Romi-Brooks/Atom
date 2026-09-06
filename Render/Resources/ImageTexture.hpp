/**
 * @file           : ImageTexture.hpp
 * @brief          : Bridges CPU-decoded image data into Renderer2D textures.
**/

#ifndef ATOM_RENDER_RESOURCES_IMAGE_TEXTURE_HPP
#define ATOM_RENDER_RESOURCES_IMAGE_TEXTURE_HPP

#include <cstddef>
#include <span>
#include <string>

#include <Media/Image/ImageDecoder.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

namespace atom::render::resources {

// Returned textures are owned by renderer. They stay valid until explicit
// DestroyTexture() or Renderer2D::Shutdown().
[[nodiscard]] auto CreateTexture(Renderer2D& renderer, const image::DecodedImage& image) -> Renderer2D::Texture*;
[[nodiscard]] auto LoadTextureFile(Renderer2D& renderer, const std::string& path) -> Renderer2D::Texture*;
[[nodiscard]] auto LoadTextureMemory(Renderer2D& renderer, std::span<const std::byte> encoded_image)
    -> Renderer2D::Texture*;

} // namespace atom::render::resources

#endif // ATOM_RENDER_RESOURCES_IMAGE_TEXTURE_HPP

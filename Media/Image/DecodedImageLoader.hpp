/**
 * @file           : DecodedImageLoader.hpp
 * @brief          : Loads and decodes images into CPU-side DecodedImage resources.
**/

#ifndef ATOM_MEDIA_IMAGE_DECODED_IMAGE_LOADER_HPP
#define ATOM_MEDIA_IMAGE_DECODED_IMAGE_LOADER_HPP

#include <memory>

#include <Asset/IResourceLoader.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Media/Image/ImageDecoder.hpp>

namespace atom::image {

// Resource payload is a CPU-side DecodedImage. GPU upload remains a separate
// step so decoding can be cached independently of Renderer2D lifetime.
class DecodedImageLoader final : public asset::TypedResourceLoader<DecodedImage> {
    public:
        [[nodiscard]] auto Kind() const -> asset::AssetKind override {
            return asset::AssetKind::Texture;
        }

    protected:
        auto LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                       std::shared_ptr<DecodedImage>& output) -> asset::AssetResult override;
};

} // namespace atom::image

#endif // ATOM_MEDIA_IMAGE_DECODED_IMAGE_LOADER_HPP

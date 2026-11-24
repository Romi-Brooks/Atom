/**
 * @file           : AudioClipResourceLoader.cpp
 * @brief          : AudioClipResourceLoader implementation.
**/

#include "AudioClipResourceLoader.hpp"

#include <Media/Audio/Resources/AudioClipLoader.hpp>

namespace atom {

auto AudioClipResourceLoader::LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                                        std::shared_ptr<atom::audio::DecodedAudio>& output) -> Result<void> {
    AudioClipLoader loader{decoders_};
    auto decoded = loader.Load(filesystem, id.Path());
    if (!decoded) {
        // Forward the loader's own reason instead of collapsing every failure
        // into one generic code: "no decoder is registered for .ogg" and "the
        // file is missing" are different problems with different fixes.
        return std::unexpected<Error>{std::move(decoded.error())};
    }
    output = std::make_shared<atom::audio::DecodedAudio>(std::move(*decoded));
    return Ok();
}

} // namespace atom

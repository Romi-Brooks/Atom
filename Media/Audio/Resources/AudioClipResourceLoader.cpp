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
        // AudioClipLoader already logged which decoder candidates it tried and
        // why each was rejected; the error here carries the identity so that log
        // line can be tied back to the resource that requested the decode.
        return std::unexpected<Error>{
            Error{ErrorCode::AudioDecoderOpenFailed, Module::Audio, "AudioClipLoader::Load"}
                .WithResource(id.Path().String())};
    }
    output = std::make_shared<atom::audio::DecodedAudio>(std::move(*decoded));
    return Ok();
}

} // namespace atom

/**
 * @file           : AudioClipResourceLoader.cpp
 * @brief          : AudioClipResourceLoader implementation.
**/

#include "AudioClipResourceLoader.hpp"

#include <Media/Audio/Resources/AudioClipLoader.hpp>

namespace atom {

auto AudioClipResourceLoader::LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                                        std::shared_ptr<atom::audio::DecodedAudio>& output)
    -> asset::AssetResult {
    AudioClipLoader loader{decoders_};
    auto decoded = loader.Load(filesystem, id.Path());
    if (!decoded)
        return asset::AssetResult::LoadFailed;
    output = std::make_shared<atom::audio::DecodedAudio>(std::move(*decoded));
    return asset::AssetResult::Success;
}

} // namespace atom

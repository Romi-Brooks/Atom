/**
 * @file           : AudioClipResourceLoader.hpp
 * @brief          : Loads a fully-decoded audio clip as a shareable resource.
**/

#ifndef ATOM_MEDIA_AUDIO_AUDIO_CLIP_RESOURCE_LOADER_HPP
#define ATOM_MEDIA_AUDIO_AUDIO_CLIP_RESOURCE_LOADER_HPP

#include <memory>

#include <Asset/IResourceLoader.hpp>
#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom::audio {
class AudioDecoderRegistry;
}

namespace atom {

// Resource payload is a fully-decoded DecodedAudio (PCM + spec). It is decoded
// once and shared: multiple consumers requesting the same ResourceId get the
// same instance via ResourceManager. Streaming playback stays with MusicPlayer;
// this loader serves the "load whole clip" path (SFX, one-shot cues) through the
// same URI/identity machinery as Script and Texture.
class AudioClipResourceLoader final : public asset::TypedResourceLoader<atom::audio::DecodedAudio> {
    public:
        explicit AudioClipResourceLoader(atom::audio::AudioDecoderRegistry& decoders) : decoders_(decoders) {}

        [[nodiscard]] auto Kind() const -> asset::AssetKind override {
            return asset::AssetKind::Audio;
        }

    protected:
        auto LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                       std::shared_ptr<atom::audio::DecodedAudio>& output) -> asset::AssetResult override;

    private:
        atom::audio::AudioDecoderRegistry& decoders_;
};

} // namespace atom

#endif // ATOM_MEDIA_AUDIO_AUDIO_CLIP_RESOURCE_LOADER_HPP

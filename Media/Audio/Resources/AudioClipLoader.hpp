#ifndef ATOM_AUDIO_CLIP_LOADER_HPP
#define ATOM_AUDIO_CLIP_LOADER_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Registry/AudioDecoderRegistry.hpp>

namespace atom {
class AudioClipLoader final {
    public:
        explicit AudioClipLoader(atom::audio::AudioDecoderRegistry& decoders) : decoders_(decoders) {}
        [[nodiscard]] auto Load(const std::string& path) const -> std::optional<atom::audio::DecodedAudio>;

        // Open a streaming decoder and return it along with the audio spec.
        // The decoder is left open — the caller takes ownership and must close it.
        // The returned atom::audio::AudioSpec is suitable for passing to atom::audio::IAudioBackend::CreateStreamingMusicSource.
        struct StreamingResult {
                std::unique_ptr<atom::audio::IAudioDecoder> decoder;
                atom::audio::AudioSpec spec;
        };
        [[nodiscard]] auto OpenStreaming(const std::string& path) const -> std::optional<StreamingResult>;

        // Same as OpenStreaming, but over an in-memory buffer (e.g. an entry
        // extracted from a resource pack). filename is used only to select a
        // decoder by extension. The buffer is borrowed: the caller must keep it
        // alive for as long as the returned decoder is used.
        [[nodiscard]] auto OpenStreamingFromMemory(const std::string& filename, const void* data,
                                                   std::size_t size) const -> std::optional<StreamingResult>;

    private:
        atom::audio::AudioDecoderRegistry& decoders_;
};
} // namespace atom

#endif // ATOM_AUDIO_CLIP_LOADER_HPP

#ifndef ATOM_AUDIO_CLIP_LOADER_HPP
#define ATOM_AUDIO_CLIP_LOADER_HPP

#include <memory>
#include <optional>
#include <string>

#include <Backend/Contracts/Audio/AudioTypes.hpp>

namespace atom {
class AudioDecoderRegistry;
class IAudioDecoder;

class AudioClipLoader final {
public:
    explicit AudioClipLoader(AudioDecoderRegistry& decoders) : decoders_(decoders) {}
    [[nodiscard]] auto Load(const std::string& path) const -> std::optional<DecodedAudio>;

    // Open a streaming decoder and return it along with the audio spec.
    // The decoder is left open — the caller takes ownership and must close it.
    // The returned AudioSpec is suitable for passing to IAudioBackend::CreateStreamingMusicSource.
    struct StreamingResult {
        std::unique_ptr<IAudioDecoder> decoder;
        AudioSpec spec;
    };
    [[nodiscard]] auto OpenStreaming(const std::string& path) const -> std::optional<StreamingResult>;

private:
    AudioDecoderRegistry& decoders_;
};
}

#endif // ATOM_AUDIO_CLIP_LOADER_HPP

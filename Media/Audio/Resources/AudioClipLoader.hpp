#ifndef ATOM_AUDIO_CLIP_LOADER_HPP
#define ATOM_AUDIO_CLIP_LOADER_HPP

#include <optional>
#include <string>

#include <Backend/Contracts/Audio/AudioTypes.hpp>

namespace atom {
class AudioDecoderRegistry;

class AudioClipLoader final {
public:
    explicit AudioClipLoader(AudioDecoderRegistry& decoders) : decoders_(decoders) {}
    [[nodiscard]] auto Load(const std::string& path) const -> std::optional<DecodedAudio>;

private:
    AudioDecoderRegistry& decoders_;
};
}

#endif // ATOM_AUDIO_CLIP_LOADER_HPP

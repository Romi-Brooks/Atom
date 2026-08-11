#ifndef ATOM_BACKEND_I_AUDIO_BACKEND_HPP
#define ATOM_BACKEND_I_AUDIO_BACKEND_HPP

#include <memory>
#include <vector>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Contracts/Audio/IAudioSource.hpp>

namespace atom {

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    [[nodiscard]] virtual auto CreateMusicSource(
        std::vector<uint8_t> pcm, const AudioSpec& spec) -> std::unique_ptr<IAudioSource> = 0;
    [[nodiscard]] virtual auto CreateSFXSource(
        const std::vector<uint8_t>& pcm, const AudioSpec& spec) -> std::unique_ptr<IAudioSource> = 0;

    // Create a streaming music source that owns an opened decoder and
    // decodes on-the-fly during playback. The caller must pass a valid,
    // opened decoder; ownership transfers to the returned source.
    [[nodiscard]] virtual auto CreateStreamingMusicSource(
        std::unique_ptr<IAudioDecoder> decoder, const AudioSpec& spec) -> std::unique_ptr<IAudioSource> = 0;
};

} // namespace atom

#endif // ATOM_BACKEND_I_AUDIO_BACKEND_HPP

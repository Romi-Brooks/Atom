#ifndef ATOM_BACKEND_I_AUDIO_BACKEND_HPP
#define ATOM_BACKEND_I_AUDIO_BACKEND_HPP

#include <memory>
#include <vector>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioSource.hpp>

namespace atom {

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    [[nodiscard]] virtual auto CreateMusicSource(
        std::vector<uint8_t> pcm, const AudioSpec& spec) -> std::unique_ptr<IAudioSource> = 0;
    [[nodiscard]] virtual auto CreateSFXSource(
        const std::vector<uint8_t>& pcm, const AudioSpec& spec) -> std::unique_ptr<IAudioSource> = 0;
};

} // namespace atom

#endif // ATOM_BACKEND_I_AUDIO_BACKEND_HPP

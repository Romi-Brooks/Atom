#ifndef ATOM_AUDIO_VOICE_POOL_HPP
#define ATOM_AUDIO_VOICE_POOL_HPP
#include <cstddef>
#include <memory>
#include <vector>
#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioBackend.hpp>
namespace atom {
class VoicePool final {
    public:
        VoicePool(atom::audio::IAudioBackend& backend, std::shared_ptr<const atom::audio::DecodedAudio> clip,
                  std::size_t maximum = 8);
        auto Acquire() -> atom::audio::IAudioSource*;
        auto StopAll() -> void;
        auto SetVolume(float volume) -> void;
        [[nodiscard]] auto FirstActive() -> atom::audio::IAudioSource*;

    private:
        auto ReapFinished() -> void;
        atom::audio::IAudioBackend& backend_;
        std::shared_ptr<const atom::audio::DecodedAudio> clip_;
        std::size_t maximum_;
        std::vector<std::unique_ptr<atom::audio::IAudioSource>> voices_;
};
} // namespace atom
#endif

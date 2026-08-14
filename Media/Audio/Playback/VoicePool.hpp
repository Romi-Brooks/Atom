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
    VoicePool(IAudioBackend& backend, std::shared_ptr<const DecodedAudio> clip, std::size_t maximum = 8);
    auto Acquire() -> IAudioSource*;
    auto StopAll() -> void;
    auto SetVolume(float volume) -> void;
    [[nodiscard]] auto FirstActive() -> IAudioSource*;

private:
    auto ReapFinished() -> void;
    IAudioBackend& backend_;
    std::shared_ptr<const DecodedAudio> clip_;
    std::size_t maximum_;
    std::vector<std::unique_ptr<IAudioSource>> voices_;
};
} // namespace atom
#endif

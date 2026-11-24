#ifndef ATOM_AUDIO_VOICE_POOL_HPP
#define ATOM_AUDIO_VOICE_POOL_HPP
#include <cstddef>
#include <memory>
#include <vector>
#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Contracts/Numeric/NumericContract.hpp>
namespace atom {
// Fixed-capacity pool of interchangeable voices for one clip. The cap is part of
// the public numeric contract (ARCH-006): it is always inside
// [numeric::min_voices, numeric::max_voices], so a caller cannot ask a backend
// for an unbounded number of streams.
//
// The constructor has two possible policies and uses both deliberately:
//  - the constructor cannot report failure, so `maximum == 0` means "use the
//    engine default" (numeric::default_voices) and an out-of-range value is
//    clamped;
//  - SetVoiceLimit() takes a caller-supplied value that *can* be rejected, so it
//    returns false and leaves the cap untouched for anything outside the range.
// Use SetVoiceLimit() when the value comes from user input or a config file.
class VoicePool final {
    public:
        VoicePool(atom::audio::IAudioBackend& backend, std::shared_ptr<const atom::audio::DecodedAudio> clip,
                  std::size_t maximum = atom::numeric::default_voices);
        auto Acquire() -> atom::audio::IAudioSource*;
        auto StopAll() -> void;
        auto SetVolume(float volume) -> void;
        [[nodiscard]] auto FirstActive() -> atom::audio::IAudioSource*;

        // Rejects a cap outside [min_voices, max_voices] and leaves the current
        // one untouched. Shrinking takes effect as voices are reaped: nothing is
        // destroyed mid-playback.
        auto SetVoiceLimit(std::size_t maximum) -> bool;
        [[nodiscard]] auto GetVoiceLimit() const -> std::size_t {
            return maximum_;
        }

    private:
        auto ReapFinished() -> void;
        atom::audio::IAudioBackend& backend_;
        std::shared_ptr<const atom::audio::DecodedAudio> clip_;
        std::size_t maximum_;
        std::vector<std::unique_ptr<atom::audio::IAudioSource>> voices_;
};
} // namespace atom
#endif // ATOM_AUDIO_VOICE_POOL_HPP

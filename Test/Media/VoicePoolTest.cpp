#include <Backend/Contracts/Audio/NullAudioBackend.hpp>
#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Numeric/NumericContract.hpp>
#include <Media/Audio/Playback/VoicePool.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <limits>
#include <memory>

auto main() -> int {
    atom::audio::NullAudioBackend backend{};
    auto clip = std::make_shared<atom::audio::DecodedAudio>();
    clip->spec = atom::audio::AudioSpec{atom::audio::AudioSampleFormat::Signed16, 44100, 2};
    clip->pcm.assign(64, 0);

    atom::VoicePool pool{backend, clip, 4};
    // Null backend cannot create sources; Acquire must fail closed.
    ATOM_CHECK(pool.Acquire() == nullptr);
    ATOM_CHECK(pool.FirstActive() == nullptr);
    pool.StopAll();
    pool.SetVolume(50.0f);
    ATOM_CHECK(pool.GetVoiceLimit() == 4);

    // Constructor vs setter policy (ARCH-006): the constructor cannot report
    // failure, so 0 means "engine default" and out-of-range values are clamped;
    // the setter takes a caller-supplied value and rejects anything outside the
    // contract range. Both behaviours are intentional and documented on VoicePool.
    atom::VoicePool tiny{backend, clip, 0};
    ATOM_CHECK(tiny.GetVoiceLimit() == atom::numeric::kDefaultVoices);
    ATOM_CHECK(tiny.Acquire() == nullptr);
    atom::VoicePool huge{backend, clip, 1'000'000};
    ATOM_CHECK(huge.GetVoiceLimit() == atom::numeric::kMaxVoices);

    ATOM_CHECK(!pool.SetVoiceLimit(0));
    ATOM_CHECK(!pool.SetVoiceLimit(atom::numeric::kMaxVoices + 1));
    ATOM_CHECK(pool.GetVoiceLimit() == 4);
    ATOM_CHECK(pool.SetVoiceLimit(atom::numeric::kMaxVoices));
    ATOM_CHECK(pool.GetVoiceLimit() == atom::numeric::kMaxVoices);
    ATOM_CHECK(pool.SetVoiceLimit(2));
    ATOM_CHECK(pool.GetVoiceLimit() == 2);

    // A non-finite volume is collapsed to silence by VoicePool::SetVolume.
    pool.SetVolume(std::numeric_limits<float>::quiet_NaN());
    return 0;
}

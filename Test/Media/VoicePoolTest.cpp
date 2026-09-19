#include <Backend/Contracts/Audio/NullAudioBackend.hpp>
#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Media/Audio/Playback/VoicePool.hpp>
#include <Test/Support/TestHelpers.hpp>

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

    // Maximum is clamped to at least one; null backend still yields nullptr.
    atom::VoicePool tiny{backend, clip, 0};
    ATOM_CHECK(tiny.Acquire() == nullptr);
    return 0;
}

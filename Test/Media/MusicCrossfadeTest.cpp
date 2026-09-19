#include <Media/Audio/Transitions/MusicCrossfade.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Backend/Contracts/Audio/NullAudioBackend.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    atom::AudioMixer mixer{};
    atom::audio::AudioDecoderRegistry decoders{};
    atom::audio::NullAudioBackend backend{};
    atom::MusicPlayer player{backend, decoders, mixer};
    atom::audio::MusicCrossfade crossfade{player};

    ATOM_CHECK(crossfade.GetState() == atom::audio::MusicTransitionState::Idle);
    ATOM_CHECK(!crossfade.IsRunning());
    ATOM_CHECK(!crossfade.Start("missing-track"));
    ATOM_CHECK(crossfade.GetState() == atom::audio::MusicTransitionState::Failed);

    atom::audio::MusicCrossfadeConfig invalid{};
    invalid.fade_out_duration = -1.0f;
    ATOM_CHECK(!crossfade.Start("anything", invalid));
    ATOM_CHECK(crossfade.GetState() == atom::audio::MusicTransitionState::Failed);

    crossfade.Reset();
    ATOM_CHECK(crossfade.GetState() == atom::audio::MusicTransitionState::Idle);
    ATOM_CHECK(!crossfade.IsRunning());
    crossfade.Cancel();
    crossfade.Stop();
    crossfade.Update(0.016f);
    ATOM_CHECK(!crossfade.IsRunning());
    return 0;
}

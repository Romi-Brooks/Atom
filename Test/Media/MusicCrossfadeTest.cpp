#include <Media/Audio/Transitions/MusicCrossfade.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Backend/Contracts/Audio/NullAudioBackend.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <limits>

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

    // ARCH-006: a NaN/Inf duration must be rejected, not carried into the fade.
    atom::audio::MusicCrossfadeConfig not_a_number{};
    not_a_number.fade_out_duration = std::numeric_limits<float>::quiet_NaN();
    ATOM_CHECK(!crossfade.Start("anything", not_a_number));
    ATOM_CHECK(crossfade.GetState() == atom::audio::MusicTransitionState::Failed);
    atom::audio::MusicCrossfadeConfig infinite{};
    infinite.fade_in_duration = std::numeric_limits<float>::infinity();
    ATOM_CHECK(!crossfade.Start("anything", infinite));
    ATOM_CHECK(crossfade.GetState() == atom::audio::MusicTransitionState::Failed);
    ATOM_CHECK(!crossfade.Switch("anything", std::numeric_limits<float>::quiet_NaN()));
    ATOM_CHECK(!crossfade.Switch("anything", std::numeric_limits<float>::infinity()));

    crossfade.Reset();
    ATOM_CHECK(crossfade.GetState() == atom::audio::MusicTransitionState::Idle);
    ATOM_CHECK(!crossfade.IsRunning());
    crossfade.Cancel();
    crossfade.Stop();
    crossfade.Update(0.016f);
    ATOM_CHECK(!crossfade.IsRunning());
    return 0;
}

#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <limits>

auto main() -> int {
    atom::AudioMixer mixer{};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    ATOM_CHECK_NEAR(mixer.GetMasterVolume(), 100.0f, 0.0f);
    ATOM_CHECK_NEAR(mixer.GetSFXVolume(), 100.0f, 0.0f);
    ATOM_CHECK_NEAR(mixer.GetMusicVolume(), 100.0f, 0.0f);
    ATOM_CHECK_NEAR(mixer.GetEffectiveSFXVolume(), 100.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(mixer.GetEffectiveMusicVolume(), 100.0f, 1.0e-5f);

    mixer.SetMasterVolume(50.0f);
    mixer.SetSFXVolume(80.0f);
    mixer.SetMusicVolume(20.0f);
    ATOM_CHECK_NEAR(mixer.GetEffectiveSFXVolume(), 40.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(mixer.GetEffectiveMusicVolume(), 10.0f, 1.0e-5f);

    mixer.SetMasterVolume(-10.0f);
    ATOM_CHECK_NEAR(mixer.GetMasterVolume(), 0.0f, 0.0f);
    mixer.SetSFXVolume(250.0f);
    ATOM_CHECK_NEAR(mixer.GetSFXVolume(), 100.0f, 0.0f);
    mixer.SetMusicVolume(250.0f);
    ATOM_CHECK_NEAR(mixer.GetMusicVolume(), 100.0f, 0.0f);

    mixer.SetMasterVolume(100.0f);
    mixer.SetSFXVolume(0.0f);
    mixer.SetMusicVolume(0.0f);
    ATOM_CHECK_NEAR(mixer.GetEffectiveSFXVolume(), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(mixer.GetEffectiveMusicVolume(), 0.0f, 0.0f);

    // ARCH-006: a non-finite volume must not survive into an effective gain.
    mixer.SetMasterVolume(100.0f);
    mixer.SetMusicVolume(nan);
    ATOM_CHECK_NEAR(mixer.GetMusicVolume(), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(mixer.GetEffectiveMusicVolume(), 0.0f, 0.0f);
    mixer.SetMusicVolume(inf);
    ATOM_CHECK_NEAR(mixer.GetMusicVolume(), 0.0f, 0.0f);
    mixer.SetSFXVolume(-inf);
    ATOM_CHECK_NEAR(mixer.GetSFXVolume(), 0.0f, 0.0f);
    mixer.SetSFXVolume(50.0f);
    ATOM_CHECK_NEAR(mixer.GetEffectiveSFXVolume(), 50.0f, 1.0e-5f);
    return 0;
}

#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    atom::AudioMixer mixer{};

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
    return 0;
}

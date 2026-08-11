#ifndef ATOM_AUDIO_MIXER_HPP
#define ATOM_AUDIO_MIXER_HPP

namespace atom {

class AudioMixer final {
public:
    auto SetMasterVolume(float volume) -> void;
    [[nodiscard]] auto GetMasterVolume() const -> float;
    auto SetSFXVolume(float volume) -> void;
    [[nodiscard]] auto GetSFXVolume() const -> float;
    auto SetMusicVolume(float volume) -> void;
    [[nodiscard]] auto GetMusicVolume() const -> float;
    [[nodiscard]] auto GetEffectiveSFXVolume() const -> float;
    [[nodiscard]] auto GetEffectiveMusicVolume() const -> float;

private:
    static auto ClampVolume(float volume) -> float;

    float master_volume_ = 100.0f;
    float sfx_volume_ = 100.0f;
    float music_volume_ = 100.0f;
};

} // namespace atom

#endif // ATOM_AUDIO_MIXER_HPP

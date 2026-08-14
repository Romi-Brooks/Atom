#include "AudioMixer.hpp"

#include <algorithm>

namespace atom {

auto AudioMixer::ClampVolume(const float volume) -> float {
    return std::clamp(volume, 0.0f, 100.0f);
}

auto AudioMixer::SetMasterVolume(const float volume) -> void {
    master_volume_ = ClampVolume(volume);
}
auto AudioMixer::GetMasterVolume() const -> float {
    return master_volume_;
}
auto AudioMixer::SetSFXVolume(const float volume) -> void {
    sfx_volume_ = ClampVolume(volume);
}
auto AudioMixer::GetSFXVolume() const -> float {
    return sfx_volume_;
}
auto AudioMixer::SetMusicVolume(const float volume) -> void {
    music_volume_ = ClampVolume(volume);
}
auto AudioMixer::GetMusicVolume() const -> float {
    return music_volume_;
}
auto AudioMixer::GetEffectiveSFXVolume() const -> float {
    return master_volume_ * sfx_volume_ / 100.0f;
}
auto AudioMixer::GetEffectiveMusicVolume() const -> float {
    return master_volume_ * music_volume_ / 100.0f;
}

} // namespace atom

#ifndef ATOM_AUDIO_EFFECT_HPP
#define ATOM_AUDIO_EFFECT_HPP

namespace atom::audio {

class IAudioSource;

// A composable high-level audio effect. Effects query optional backend
// capabilities themselves, so one effect object can safely be used with any
// registered audio backend.
class IAudioEffect {
    public:
        virtual ~IAudioEffect() = default;
        virtual auto Apply(IAudioSource& source) -> void = 0;
};

} // namespace atom::audio

#endif // ATOM_AUDIO_EFFECT_HPP

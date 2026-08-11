#ifndef ATOM_BACKEND_RUNTIME_I_AUDIO_BACKEND_CHANGE_LISTENER_HPP
#define ATOM_BACKEND_RUNTIME_I_AUDIO_BACKEND_CHANGE_LISTENER_HPP

namespace atom {

class IAudioBackendChangeListener {
public:
    virtual ~IAudioBackendChangeListener() = default;
    virtual auto OnAudioBackendChanging() -> void = 0;
    virtual auto OnAudioDecoderBackendChanging() -> void = 0;
};

} // namespace atom

#endif

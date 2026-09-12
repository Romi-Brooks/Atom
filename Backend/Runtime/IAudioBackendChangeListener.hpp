#ifndef ATOM_BACKEND_RUNTIME_I_AUDIO_BACKEND_CHANGE_LISTENER_HPP
#define ATOM_BACKEND_RUNTIME_I_AUDIO_BACKEND_CHANGE_LISTENER_HPP

namespace atom::backend {

class IAudioBackendChangeListener {
    public:
        virtual ~IAudioBackendChangeListener() = default;

        // Called before the active backend is replaced. Listeners must stop and
        // release every source/ID they hold here: the runtime detaches whatever
        // is left and then destroys the old backend. No playback position is
        // migrated, and dropped IDs are not re-registered automatically.
        virtual auto OnAudioBackendChanging() -> void = 0;

        // Called after the replacement backend is live and the runtime's
        // generation counter has advanced. Listeners use this to invalidate
        // caches whose contents were produced by the previous backend and to
        // schedule their own reload. Default no-op so existing listeners keep
        // compiling.
        virtual auto OnAudioBackendChanged() -> void {}
};

} // namespace atom::backend

#endif

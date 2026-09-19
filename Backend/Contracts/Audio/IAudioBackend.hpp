#ifndef ATOM_BACKEND_I_AUDIO_BACKEND_HPP
#define ATOM_BACKEND_I_AUDIO_BACKEND_HPP

#include <memory>
#include <vector>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Contracts/Audio/IAudioSource.hpp>

namespace atom::audio {

class IAudioBackend {
    public:
        virtual ~IAudioBackend() = default;

        [[nodiscard]] virtual auto CreateMusicSource(std::vector<uint8_t> pcm, const AudioSpec& spec)
            -> std::unique_ptr<IAudioSource> = 0;
        [[nodiscard]] virtual auto CreateSFXSource(const std::vector<uint8_t>& pcm, const AudioSpec& spec)
            -> std::unique_ptr<IAudioSource> = 0;

        // Create a streaming music source that owns an opened decoder and
        // decodes on-the-fly during playback. The caller must pass a valid,
        // opened decoder; ownership transfers to the returned source.
        [[nodiscard]] virtual auto CreateStreamingMusicSource(std::unique_ptr<IAudioDecoder> decoder,
                                                              const AudioSpec& spec)
            -> std::unique_ptr<IAudioSource> = 0;

        // Release every source this backend created before the backend itself is
        // destroyed. BackendRuntime calls this as part of SetAudioBackend; the
        // backend forwards it to its AudioSourceRegistry (see
        // AudioSourceRegistry.hpp) so that no source outlives its backend.
        //
        // The default no-op is only correct for backends that own no platform
        // resources (test/fake/null backends). A backend that owns a process-wide
        // subsystem -- the SDL3 backends, whose teardown runs SDL_QuitSubSystem and
        // destroys every audio stream in the process -- must implement this.
        virtual auto Quiesce() -> void {}
};

} // namespace atom::audio

#endif // ATOM_BACKEND_I_AUDIO_BACKEND_HPP

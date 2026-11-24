#ifndef ATOM_NULL_AUDIO_BACKEND_HPP
#define ATOM_NULL_AUDIO_BACKEND_HPP

#include <memory>
#include <vector>

#include <Backend/Contracts/Audio/IAudioBackend.hpp>

namespace atom::audio {

// Stand-in backend used by BackendRuntime while no playback backend is active.
//
// BackendRuntime::Audio() used to throw std::runtime_error in that state, which
// turned an audio-backend switch into an uncaught exception (and therefore
// std::terminate) for any background loader thread that asked for the backend
// mid-switch. Every factory here reports "creation failed" instead, so callers
// keep their normal null-check path.
class NullAudioBackend final : public IAudioBackend {
    public:
        [[nodiscard]] auto CreateMusicSource(std::vector<uint8_t>, const AudioSpec&)
            -> std::unique_ptr<IAudioSource> override {
            return nullptr;
        }

        [[nodiscard]] auto CreateSFXSource(const std::vector<uint8_t>&, const AudioSpec&)
            -> std::unique_ptr<IAudioSource> override {
            return nullptr;
        }

        [[nodiscard]] auto CreateStreamingMusicSource(std::unique_ptr<IAudioDecoder>, const AudioSpec&)
            -> std::unique_ptr<IAudioSource> override {
            return nullptr;
        }
};

} // namespace atom::audio

#endif // ATOM_NULL_AUDIO_BACKEND_HPP

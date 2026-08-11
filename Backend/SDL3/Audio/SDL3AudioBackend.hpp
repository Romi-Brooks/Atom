#ifndef ATOM_BACKEND_SDL3_AUDIO_BACKEND_HPP
#define ATOM_BACKEND_SDL3_AUDIO_BACKEND_HPP

#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>

namespace atom {

class SDL3AudioBackend final : public IAudioBackend {
public:
    [[nodiscard]] auto CreateMusicSource(
        std::vector<uint8_t> pcm, const AudioSpec& spec) -> std::unique_ptr<IAudioSource> override;
    [[nodiscard]] auto CreateSFXSource(
        const std::vector<uint8_t>& pcm, const AudioSpec& spec) -> std::unique_ptr<IAudioSource> override;
    [[nodiscard]] auto IsReady() const -> bool;
private:
    SDLSubsystemLease audio_runtime_{SDLSubsystem::Audio};
};

} // namespace atom

#endif

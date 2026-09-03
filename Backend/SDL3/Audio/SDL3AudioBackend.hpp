#ifndef ATOM_BACKEND_SDL3_AUDIO_BACKEND_HPP
#define ATOM_BACKEND_SDL3_AUDIO_BACKEND_HPP

#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>

namespace atom::backend::sdl3 {

class SDL3AudioBackend final : public atom::audio::IAudioBackend {
    public:
        [[nodiscard]] auto CreateMusicSource(std::vector<uint8_t> pcm, const atom::audio::AudioSpec& spec)
            -> std::unique_ptr<atom::audio::IAudioSource> override;
        // Create a streaming music source backed by an opened atom::audio::IAudioDecoder.
        // The decoder is consumed (moved) — it will be closed by the source on destruction.
        [[nodiscard]] auto CreateStreamingMusicSource(std::unique_ptr<atom::audio::IAudioDecoder> decoder,
                                                      const atom::audio::AudioSpec& spec)
            -> std::unique_ptr<atom::audio::IAudioSource> override;
        [[nodiscard]] auto CreateSFXSource(const std::vector<uint8_t>& pcm, const atom::audio::AudioSpec& spec)
            -> std::unique_ptr<atom::audio::IAudioSource> override;
        [[nodiscard]] auto IsReady() const -> bool;

    private:
        SDLSubsystemLease audio_runtime_{SDLSubsystem::Audio};
};

} // namespace atom::backend::sdl3

#endif

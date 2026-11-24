#ifndef ATOM_SDL3_MIXER_AUDIO_BACKEND_HPP
#define ATOM_SDL3_MIXER_AUDIO_BACKEND_HPP

#include <memory>

#include <Backend/Contracts/Audio/AudioSourceRegistry.hpp>
#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>

namespace atom::backend::sdl3mixer {

class SDL3MixerContext;

class SDL3MixerAudioBackend final : public atom::audio::IAudioBackend {
    public:
        SDL3MixerAudioBackend();

        [[nodiscard]] auto CreateMusicSource(std::vector<uint8_t> pcm, const atom::audio::AudioSpec& spec)
            -> std::unique_ptr<atom::audio::IAudioSource> override;
        [[nodiscard]] auto CreateStreamingMusicSource(std::unique_ptr<atom::audio::IAudioDecoder> decoder,
                                                      const atom::audio::AudioSpec& spec)
            -> std::unique_ptr<atom::audio::IAudioSource> override;
        [[nodiscard]] auto CreateSFXSource(const std::vector<uint8_t>& pcm, const atom::audio::AudioSpec& spec)
            -> std::unique_ptr<atom::audio::IAudioSource> override;
        [[nodiscard]] auto IsReady() const -> bool;

        // Stops and releases every source this backend created before the shared
        // mixer context is released. See SDL3AudioBackend::Quiesce().
        auto Quiesce() -> void override;

    private:
        sdl3::SDLSubsystemLease audio_runtime_{sdl3::SDLSubsystem::Audio};
        std::shared_ptr<SDL3MixerContext> context_;
        atom::audio::AudioSourceRegistry sources_;
};

} // namespace atom::backend::sdl3mixer

#endif // ATOM_SDL3_MIXER_AUDIO_BACKEND_HPP

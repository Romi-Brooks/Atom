#ifndef ATOM_SDL3_SFX_SOURCE_HPP
#define ATOM_SDL3_SFX_SOURCE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Audio/AudioSourceRegistry.hpp>

namespace atom::backend::sdl3 {

// Buffered one-shot/looping sound effect voice on native SDL3.
//
// Data is handed to the device from SDL's audio thread through a stream
// callback instead of a single up-front push. That keeps looping exact (the
// callback is the only writer of the read cursor) and makes completion
// observable: drained_ is only set once every byte has been handed over.
class SDL3SFXSource final : public atom::audio::BackendOwnedSource {
    public:
        SDL3SFXSource();
        ~SDL3SFXSource() override;

        auto Play() -> void override;
        auto Stop() -> void override;
        auto Pause() -> void override;
        [[nodiscard]] auto GetState() const -> atom::audio::AudioSourceState override;
        auto SetVolume(float volume) -> void override;
        [[nodiscard]] auto GetVolume() const -> float override;
        auto SetLooping(bool loop) -> void override;
        [[nodiscard]] auto IsLooping() const -> bool override;
        auto SetPlayingOffset(float seconds) -> bool override;
        [[nodiscard]] auto GetPlayingOffset() const -> float override;
        [[nodiscard]] auto IsSeekable() const -> bool override;
        [[nodiscard]] auto IsFinished() const -> bool override;
        auto SetBuffer(const uint8_t* data, uint32_t length) -> void override;

        // Set the audio format spec (must be called before Play, alongside SetBuffer)
        auto SetSpec(const SDL_AudioSpec& spec) -> void;

    protected:
        auto ReleaseBackendHandles() -> void override;

    private:
        auto EnsureStream() -> bool;
        // Runs on SDL's audio thread; must not log or block.
        static void SDLCALL FeedCallback(void* userdata, SDL_AudioStream* stream, int additional_amount,
                                         int total_amount);
        auto Feed(SDL_AudioStream& stream, int additional_amount) -> void;

        SDL_AudioStream* stream_ = nullptr;
        std::vector<uint8_t> pcm_data_;
        SDL_AudioSpec spec_{};
        std::atomic<float> volume_{100.0f};
        std::atomic<bool> looping_{false};
        std::atomic<atom::audio::AudioSourceState> state_{atom::audio::AudioSourceState::Stopped};
        // Byte offset of the next sample handed to SDL. Written by the audio
        // thread, read by GetPlayingOffset()/SetPlayingOffset().
        std::atomic<std::size_t> feed_cursor_{0};
        // Set by the audio thread once every byte has been handed over and the
        // source is not looping.
        std::atomic<bool> drained_{false};
        // Set by SetPlayingOffset() so the next Play() keeps the seek target
        // instead of rewinding to the beginning.
        std::atomic<bool> seek_pending_{false};
};

} // namespace atom::backend::sdl3

#endif // ATOM_SDL3_SFX_SOURCE_HPP

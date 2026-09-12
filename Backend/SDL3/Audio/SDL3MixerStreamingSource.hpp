#ifndef ATOM_SDL3_MIXER_STREAMING_SOURCE_HPP
#define ATOM_SDL3_MIXER_STREAMING_SOURCE_HPP

#include <atomic>
#include <memory>
#include <thread>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Audio/AudioExtensions.hpp>
#include <Backend/Contracts/Audio/AudioSourceRegistry.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

struct MIX_Track;

namespace atom::backend::sdl3mixer {

class SDL3MixerContext;

// Streaming SDL_mixer source: an atom decoder feeds an SDL_AudioStream that a
// MIX_Track plays. This is the source MusicPlayer creates, so it carries the
// same optional spatial extension set as SDL3MixerSource -- music must be able
// to participate in 3D spatial audio exactly like SFX do.
class SDL3MixerStreamingSource final : public atom::audio::BackendOwnedSource,
                                       public atom::audio::IPitchControl,
                                       public atom::audio::IPanControl,
                                       public atom::audio::ISpatialEmitter {
    public:
        SDL3MixerStreamingSource(std::shared_ptr<SDL3MixerContext> context,
                                 std::unique_ptr<atom::audio::IAudioDecoder> decoder, const SDL_AudioSpec& spec);
        ~SDL3MixerStreamingSource() override;

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

        auto SetPitch(float ratio) -> void override;
        [[nodiscard]] auto GetPitch() const -> float override;
        auto SetPan(float pan) -> void override;
        [[nodiscard]] auto GetPan() const -> float override;
        auto SetPosition(const atom::audio::AudioPosition& position) -> void override;
        auto ClearPosition() -> void override;
        [[nodiscard]] auto HasPosition() const -> bool override;
        [[nodiscard]] auto IsValid() const -> bool;

    protected:
        auto ReleaseBackendHandles() -> void override;

    private:
        auto DecodeLoop() -> void;
        auto ApplyPan() -> void;
        // Stops the producer thread and drops everything buffered, leaving the
        // decoder untouched at its current position.
        auto HaltDecoding() -> void;
        // (Re)starts the producer thread and the MIX track from the decoder's
        // current position.
        auto BeginDecoding() -> void;

        std::shared_ptr<SDL3MixerContext> context_;
        std::unique_ptr<atom::audio::IAudioDecoder> decoder_;
        MIX_Track* track_ = nullptr;
        SDL_AudioStream* stream_ = nullptr;
        SDL_AudioSpec spec_{};

        std::atomic<atom::audio::AudioSourceState> state_{atom::audio::AudioSourceState::Stopped};
        std::atomic<float> volume_{100.0f};
        std::atomic<bool> loop_{false};
        std::atomic<bool> running_{false};
        std::atomic<bool> finished_{false};

        // Absolute frame the current run starts at (the last seek target). The
        // reported offset is this base plus the frames the mixer has consumed.
        std::atomic<std::uint64_t> position_base_frames_{0};
        // Input frames pushed into the SDL stream during the current run. The
        // mixer forces the stream *output* to F32, so progress is measured on the
        // input side (pushed minus still-queued), which is also the domain the
        // decode loop throttles on.
        std::atomic<std::uint64_t> frames_pushed_{0};
        // True while the decoder sits at a position set by SetPlayingOffset() and
        // the next Play() must not rewind over it.
        std::atomic<bool> decoder_positioned_{false};

        std::atomic<float> pitch_{1.0f};
        std::atomic<float> pan_{0.0f};
        std::atomic<bool> positioned_{false};
        std::atomic<bool> pan_requested_{false};

        std::thread worker_;
};

} // namespace atom::backend::sdl3mixer

#endif // ATOM_SDL3_MIXER_STREAMING_SOURCE_HPP

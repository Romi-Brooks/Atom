#ifndef ATOM_SDL3_MIXER_SOURCE_HPP
#define ATOM_SDL3_MIXER_SOURCE_HPP

#include <atomic>
#include <cstddef>
#include <memory>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Audio/AudioExtensions.hpp>
#include <Backend/Contracts/Audio/AudioSourceRegistry.hpp>

struct MIX_Audio;
struct MIX_Track;

namespace atom::backend::sdl3mixer {

class SDL3MixerContext;

// Fully buffered SDL_mixer source (SFX voices and short music clips).
//
// The mixer backend is the one that offers the optional spatial extension set:
// pitch, manual stereo pan and 3D position.
class SDL3MixerSource final : public atom::audio::BackendOwnedSource,
                              public atom::audio::IPitchControl,
                              public atom::audio::IPanControl,
                              public atom::audio::ISpatialEmitter {
    public:
        SDL3MixerSource(std::shared_ptr<SDL3MixerContext> context, const uint8_t* pcm, std::size_t length,
                        const SDL_AudioSpec& spec);
        ~SDL3MixerSource() override;

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
        auto ApplyPan() -> void;
        auto PlayOptions() const -> SDL_PropertiesID;

        std::shared_ptr<SDL3MixerContext> context_;
        MIX_Track* track_ = nullptr;
        MIX_Audio* audio_ = nullptr;
        SDL_AudioSpec spec_{};
        std::atomic<float> volume_{100.0f};
        std::atomic<bool> looping_{false};
        std::atomic<float> pitch_{1.0f};
        std::atomic<float> pan_{0.0f};
        std::atomic<bool> positioned_{false};
        // True once the caller asked for a specific pan. ClearPosition() uses it
        // to restore the pan after leaving 3D spatialization.
        std::atomic<bool> pan_requested_{false};
};

} // namespace atom::backend::sdl3mixer

#endif // ATOM_SDL3_MIXER_SOURCE_HPP

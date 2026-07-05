#ifndef ATOM_SDL3_SFX_SOURCE_HPP
#define ATOM_SDL3_SFX_SOURCE_HPP

#include <SDL3/SDL.h>
#include <Engine/Interfaces/IAudioSource.hpp>

#include <cstdint>
#include <vector>

namespace atom {

class SDL3SFXSource : public IAudioSource {
public:
    SDL3SFXSource();
    ~SDL3SFXSource() override;

    SDL3SFXSource(const SDL3SFXSource&) = delete;
    auto operator=(const SDL3SFXSource&) -> SDL3SFXSource& = delete;

    auto Play() -> void override;
    auto Stop() -> void override;
    auto Pause() -> void override;
    [[nodiscard]] auto GetState() const -> AudioSourceState override;
    auto SetVolume(float volume) -> void override;
    [[nodiscard]] auto GetVolume() const -> float override;
    auto SetLooping(bool loop) -> void override;
    [[nodiscard]] auto IsLooping() const -> bool override;
    auto SetPlayingOffset(float seconds) -> void override;
    [[nodiscard]] auto GetPlayingOffset() const -> float override;
    auto SetBuffer(const uint8_t* data, uint32_t length) -> void override;

    // Set the audio format spec (must be called before Play, alongside SetBuffer)
    auto SetSpec(const SDL_AudioSpec& spec) -> void;

private:
    SDL_AudioStream* stream_ = nullptr;
    std::vector<uint8_t> pcm_data_;
    SDL_AudioSpec spec_{};
    float volume_ = 100.0f;
    bool looping_ = false;
    AudioSourceState state_ = AudioSourceState::Stopped;

    auto EnsureStream() -> bool;
};

} // namespace atom

#endif // ATOM_SDL3_SFX_SOURCE_HPP

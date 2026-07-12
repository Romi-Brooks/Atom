#ifndef ATOM_SDL3_AUDIO_BUFFER_HPP
#define ATOM_SDL3_AUDIO_BUFFER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Engine/Interfaces/IAudioBuffer.hpp>

namespace atom {

class SDL3AudioBuffer : public IAudioBuffer {
public:
    SDL3AudioBuffer() = default;
    ~SDL3AudioBuffer() override;

    SDL3AudioBuffer(const SDL3AudioBuffer&) = delete;
    auto operator=(const SDL3AudioBuffer&) -> SDL3AudioBuffer& = delete;

    auto LoadFromFile(const std::string& path) -> bool override;
    auto LoadFromMemory(const uint8_t* data, uint32_t size) -> bool override;

    // Set the audio format spec (used when loading via IAudioDecoder instead
    // of SDL_LoadWAV).  Must be called after LoadFromMemory().
    auto SetAudioSpec(const SDL_AudioSpec& spec) -> void;

    [[nodiscard]] auto GetSampleRate() const -> uint32_t override;
    [[nodiscard]] auto GetChannelCount() const -> uint8_t override;
    [[nodiscard]] auto GetFormat() const -> uint32_t override;
    [[nodiscard]] auto GetDuration() const -> float override;
    [[nodiscard]] auto GetSamples() const -> const int16_t* override;
    [[nodiscard]] auto GetSampleCount() const -> uint64_t override;

private:
    std::vector<uint8_t> pcm_data_;
    SDL_AudioSpec spec_{};
};

} // namespace atom

#endif // ATOM_SDL3_AUDIO_BUFFER_HPP

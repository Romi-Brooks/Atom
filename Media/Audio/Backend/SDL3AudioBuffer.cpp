#include "SDL3AudioBuffer.hpp"

#include <SDL3/SDL.h>

namespace atom {

SDL3AudioBuffer::~SDL3AudioBuffer() = default;

auto SDL3AudioBuffer::LoadFromFile(const std::string& path) -> bool {
    uint8_t* wav_data = nullptr;
    uint32_t wav_length = 0;
    SDL_AudioSpec spec{};

    if (!SDL_LoadWAV(path.c_str(), &spec, &wav_data, &wav_length)) {
        return false;
    }

    spec_ = spec;
    pcm_data_.assign(wav_data, wav_data + wav_length);
    SDL_free(wav_data);
    return true;
}

auto SDL3AudioBuffer::LoadFromMemory(const uint8_t* data, uint32_t size) -> bool {
    // For pre-decoded PCM data, store as-is.
    // Caller must provide the format via internal spec (set separately).
    pcm_data_.assign(data, data + size);
    return true;
}

auto SDL3AudioBuffer::GetSampleRate() const -> uint32_t {
    return spec_.freq;
}

auto SDL3AudioBuffer::GetChannelCount() const -> uint8_t {
    return static_cast<uint8_t>(spec_.channels);
}

auto SDL3AudioBuffer::GetDuration() const -> float {
    if (spec_.freq == 0 || spec_.channels == 0) return 0.0f;
    const auto bytes_per_sample = static_cast<float>(SDL_AUDIO_BYTESIZE(spec_.format));
    return static_cast<float>(pcm_data_.size()) /
           (static_cast<float>(spec_.freq) * static_cast<float>(spec_.channels) * bytes_per_sample);
}

auto SDL3AudioBuffer::GetSamples() const -> const int16_t* {
    return reinterpret_cast<const int16_t*>(pcm_data_.data());
}

auto SDL3AudioBuffer::GetSampleCount() const -> uint64_t {
    return pcm_data_.size() / sizeof(int16_t);
}

} // namespace atom

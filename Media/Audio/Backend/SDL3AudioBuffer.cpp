#include <SDL3/SDL.h>

#include <Log/LogSystem.hpp>

#include "SDL3AudioBuffer.hpp"

namespace atom {

SDL3AudioBuffer::~SDL3AudioBuffer() = default;

auto SDL3AudioBuffer::LoadFromFile(const std::string& path) -> bool {
    uint8_t* wav_data = nullptr;
    uint32_t wav_length = 0;
    SDL_AudioSpec spec{};

    if (!SDL_LoadWAV(path.c_str(), &spec, &wav_data, &wav_length)) {
        LOG_ERROR(atom::LogChannel::SDL_BACKEND_AUDIO,
                  "SDL_LoadWAV failed: " + std::string(SDL_GetError()) + " for: " + path);
        return false;
    }

    spec_ = spec;
    pcm_data_.assign(wav_data, wav_data + wav_length);
    SDL_free(wav_data);

    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO,
              "Loaded WAV: fmt=" + std::to_string(spec_.format) +
              " freq=" + std::to_string(spec_.freq) +
              " ch=" + std::to_string(spec_.channels) +
              " samples=" + std::to_string(GetSampleCount()));

    return true;
}

auto SDL3AudioBuffer::LoadFromMemory(const uint8_t* data, uint32_t size) -> bool {
    pcm_data_.assign(data, data + size);
    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO,
              "Loaded " + std::to_string(size) + " bytes from memory");
    return true;
}

auto SDL3AudioBuffer::SetAudioSpec(const SDL_AudioSpec& spec) -> void {
    spec_ = spec;
    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO,
              "SetAudioSpec: fmt=" + std::to_string(spec.format) +
              " freq=" + std::to_string(spec.freq) +
              " ch=" + std::to_string(spec.channels));
}

auto SDL3AudioBuffer::GetSampleRate() const -> uint32_t {
    return spec_.freq;
}

auto SDL3AudioBuffer::GetChannelCount() const -> uint8_t {
    return static_cast<uint8_t>(spec_.channels);
}

auto SDL3AudioBuffer::GetFormat() const -> uint32_t {
    return spec_.format;
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
    const auto bytes_per_sample = SDL_AUDIO_BYTESIZE(spec_.format);
    return bytes_per_sample > 0 ? pcm_data_.size() / bytes_per_sample : 0;
}

} // namespace atom

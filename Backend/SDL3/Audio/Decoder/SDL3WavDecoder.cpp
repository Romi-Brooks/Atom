/**
  * @file           : SDL3WavDecoder.cpp
  * @author         : Romi Brooks
  * @brief          : SDL_LoadWAV-based WAV decoder backend
  * @attention      : Uses SDL_LoadWAV for correct UTF-8 path handling.
  * @date           : 2026/7/5
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <algorithm>

#include <SDL3/SDL.h>

#include <Log/LogSystem.hpp>

#include "SDL3WavDecoder.hpp"

namespace atom {

SDL3WavDecoder::~SDL3WavDecoder() {
    Close();
}

auto SDL3WavDecoder::Open(const std::string& path) -> bool {
    Close();

    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO,
              "Opening WAV file: " + path);

    uint8_t* wav_data = nullptr;
    uint32_t wav_length = 0;
    SDL_AudioSpec spec{};

    if (!SDL_LoadWAV(path.c_str(), &spec, &wav_data, &wav_length)) {
        LOG_ERROR(atom::LogChannel::SDL_BACKEND_AUDIO,
                  "SDL_LoadWAV failed: " + std::string(SDL_GetError()) + " for: " + path);
        return false;
    }

    pcm_data_.assign(wav_data, wav_data + wav_length);
    SDL_free(wav_data);
    read_cursor_ = 0;

    info_.sample_rate    = static_cast<uint32_t>(spec.freq);
    info_.channels       = static_cast<uint16_t>(spec.channels);
    info_.bits_per_sample = static_cast<uint16_t>(SDL_AUDIO_BITSIZE(spec.format));
    info_.is_float       = SDL_AUDIO_ISFLOAT(spec.format) != 0;

    const auto bytes_per_sample = info_.bits_per_sample / 8;
    if (bytes_per_sample == 0 || info_.channels == 0) {
        LOG_ERROR(atom::LogChannel::SDL_BACKEND_AUDIO,
                  "Invalid format: bps=" + std::to_string(info_.bits_per_sample) +
                  " ch=" + std::to_string(info_.channels));
        pcm_data_.clear();
        return false;
    }
    info_.total_pcm_frames = pcm_data_.size() / (bytes_per_sample * info_.channels);

    LOG_INFO(atom::LogChannel::SDL_BACKEND_AUDIO,
             "Loaded WAV: rate=" + std::to_string(info_.sample_rate) +
             " ch=" + std::to_string(info_.channels) +
             " bits=" + std::to_string(info_.bits_per_sample) +
             " frames=" + std::to_string(info_.total_pcm_frames) +
             (info_.is_float ? " float" : ""));
    return true;
}

auto SDL3WavDecoder::Close() -> void {
    if (!pcm_data_.empty()) {
        LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "Closing decoder");
    }
    pcm_data_.clear();
    read_cursor_ = 0;
    info_ = DecoderInfo{};
}

auto SDL3WavDecoder::DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t {
    if (read_cursor_ >= pcm_data_.size()) return 0;

    const auto remaining = pcm_data_.size() - read_cursor_;
    const auto to_copy = (std::min)(static_cast<uint64_t>(max_bytes), remaining);

    std::copy(pcm_data_.begin() + static_cast<std::ptrdiff_t>(read_cursor_),
              pcm_data_.begin() + static_cast<std::ptrdiff_t>(read_cursor_ + to_copy),
              output);
    read_cursor_ += to_copy;

    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO,
              "DecodeChunk: requested=" + std::to_string(max_bytes) +
              " copied=" + std::to_string(to_copy) +
              " cursor=" + std::to_string(read_cursor_) +
              "/" + std::to_string(pcm_data_.size()));
    return static_cast<uint32_t>(to_copy);
}

auto SDL3WavDecoder::Rewind() -> bool {
    read_cursor_ = 0;
    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "Rewind to start");
    return true;
}

auto SDL3WavDecoder::GetInfo() const -> const atom::DecoderInfo& {
    return info_;
}

auto SDL3WavDecoder::IsOpen() const -> bool {
    return !pcm_data_.empty();
}

} // namespace atom

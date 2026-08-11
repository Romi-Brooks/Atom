/**
  * @file           : SDL3WavDecoder.hpp
  * @author         : Romi Brooks
  * @brief          : IAudioDecoder implementation wrapping SDL_LoadWAV
  * @attention      : Uses SDL_LoadWAV internally for correct UTF-8 path
  *                   handling on Windows. The RiffWaveReader (dependency-free)
  *                   is available for non-SDL contexts.
  * @date           : 2026/7/8
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDL3_WAV_DECODER_HPP
#define ATOM_BACKEND_SDL3_WAV_DECODER_HPP

#include <cstdint>
#include <vector>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom {

class SDL3WavDecoder : public atom::IAudioDecoder {
public:
    SDL3WavDecoder() = default;
    ~SDL3WavDecoder() override;

    SDL3WavDecoder(const SDL3WavDecoder&) = delete;
    auto operator=(const SDL3WavDecoder&) -> SDL3WavDecoder& = delete;

    // IAudioDecoder
    auto Open(const std::string& path) -> bool override;
    auto Close() -> void override;
    auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t override;
    auto Rewind() -> bool override;
    [[nodiscard]] auto GetInfo() const -> const atom::DecoderInfo& override;
    [[nodiscard]] auto IsOpen() const -> bool override;

private:
    atom::DecoderInfo info_{};
    std::vector<uint8_t> pcm_data_;
    uint64_t read_cursor_ = 0;
};

} // namespace atom

#endif // ATOM_BACKEND_SDL3_WAV_DECODER_HPP

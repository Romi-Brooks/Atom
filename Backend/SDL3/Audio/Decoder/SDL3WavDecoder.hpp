/**
  * @file           : SDL3WavDecoder.hpp
  * @author         : Romi Brooks
  * @brief          : Streaming WAV decoder backed by SDL_IOStream
  * @attention      : Keeps the file open and reads PCM in bounded chunks.
  * @date           : 2026/7/8
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDL3_WAV_DECODER_HPP
#define ATOM_BACKEND_SDL3_WAV_DECODER_HPP

#include <cstdint>
#include <vector>
#include <SDL3/SDL_iostream.h>

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
    SDL_IOStream* io_ = nullptr;
    Sint64 data_offset_ = 0;
    uint64_t data_size_ = 0;
    uint64_t bytes_read_ = 0;
    uint16_t source_bits_per_sample_ = 0;
    std::vector<uint8_t> decode_scratch_;
};

} // namespace atom

#endif // ATOM_BACKEND_SDL3_WAV_DECODER_HPP

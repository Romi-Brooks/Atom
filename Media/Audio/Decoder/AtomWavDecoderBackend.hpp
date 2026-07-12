/**
  * @file           : AtomWavDecoderBackend.hpp
  * @author         : Romi Brooks
  * @brief          : IAudioDecoder implementation wrapping SDL_LoadWAV
  * @attention      : Uses SDL_LoadWAV internally for correct UTF-8 path
  *                   handling on Windows. The WavDecoder (dependency-free)
  *                   is available for non-SDL contexts.
  * @date           : 2026/7/8
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ATOM_WAV_DECODER_BACKEND_HPP
#define ATOM_ATOM_WAV_DECODER_BACKEND_HPP

#include <cstdint>
#include <vector>

#include <Engine/Interfaces/IAudioDecoder.hpp>

namespace atom {

class AtomWavDecoderBackend : public atom::IAudioDecoder {
public:
    AtomWavDecoderBackend() = default;
    ~AtomWavDecoderBackend() override;

    AtomWavDecoderBackend(const AtomWavDecoderBackend&) = delete;
    auto operator=(const AtomWavDecoderBackend&) -> AtomWavDecoderBackend& = delete;

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

#endif // ATOM_ATOM_WAV_DECODER_BACKEND_HPP

/**
  * @file           : WavRiffDecoder.hpp
  * @author         : Romi Brooks
  * @brief          : The engine's single WAV decoder, backed by RiffWaveReader
  * @attention      : Handles uncompressed PCM (8/16/24/32-bit) and IEEE float
  *                   (32-bit). 24-bit packed PCM is expanded to S32 because SDL
  *                   has no native packed 24-bit format.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_WAVRIFF_DECODER_HPP
#define ATOM_BACKEND_WAVRIFF_DECODER_HPP

#include <cstdint>
#include <vector>

#include <Backend/Builtin/Audio/Decoder/WavRiff/RiffWaveReader.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom {

class WavRiffDecoder final : public IAudioDecoder {
public:
    auto Open(const std::string& path) -> bool override;
    auto Close() -> void override;
    auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t override;
    auto Rewind() -> bool override;
    [[nodiscard]] auto GetInfo() const -> const DecoderInfo& override;
    [[nodiscard]] auto IsOpen() const -> bool override;

private:
    RiffWaveReader reader_;
    DecoderInfo info_{};
    uint16_t source_bits_per_sample_ = 0;
    std::vector<uint8_t> decode_scratch_;
};

} // namespace atom

#endif // ATOM_BACKEND_WAVRIFF_DECODER_HPP

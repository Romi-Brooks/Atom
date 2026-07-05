/**
  * @file           : IAudioDecoder.hpp
  * @author         : Romi Brooks
  * @brief          : Abstract interface for audio decoders (Strategy pattern)
  * @attention      : Implementations: SDL3 WAV backend, Atom WavDecoder, etc.
  *                   Each decoder opens a file and produces raw PCM chunks.
  * @date           : 2026/7/5
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_IAUDIO_DECODER_HPP
#define ATOM_IAUDIO_DECODER_HPP

#include <cstdint>
#include <string>

namespace atom {

struct DecoderInfo {
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint64_t total_pcm_frames = 0; // 0 = unknown (streaming)
};

class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;

    // Open a file for decoding. Returns false if format not supported.
    virtual auto Open(const std::string& path) -> bool = 0;

    // Close and release all resources.
    virtual auto Close() -> void = 0;

    // Decode the next chunk of PCM data.
    // Returns number of bytes written to output. 0 = EOF or error.
    virtual auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t = 0;

    // Seek to the beginning of PCM data (rewind).
    virtual auto Rewind() -> bool = 0;

    // Get decoder info (valid after Open succeeds).
    [[nodiscard]] virtual auto GetInfo() const -> const DecoderInfo& = 0;

    // Check if a file is open.
    [[nodiscard]] virtual auto IsOpen() const -> bool = 0;
};

} // namespace atom

#endif // ATOM_IAUDIO_DECODER_HPP

/**
  * @file           : RiffWaveReader.hpp
  * @author         : Romi Brooks
  * @brief          : C++ WAV PCM RIFF decoder (builtin)
  * @attention      : Self-contained RIFF/WAV parser. No external dependencies.
  *                   Based on the original WavProf C decoder, rewritten in C++.
  * @date           : 2026/7/8
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_BUILTIN_RIFF_WAVE_READER_HPP
#define ATOM_BACKEND_BUILTIN_RIFF_WAVE_READER_HPP

#include <cstdint>
#include <cstdio>
#include <string>

namespace atom {

struct WavHeader {
    char     chunk_id[4]{};       // "RIFF"
    uint32_t chunk_size = 0;      // file size - 8
    char     format[4]{};         // "WAVE"
    char     subchunk_id[4]{};    // "fmt "
    uint32_t subchunk_size = 0;   // fmt chunk size (16 for PCM)
    uint16_t audio_format = 0;    // 1 = PCM
    uint16_t num_channels = 0;
    uint32_t sample_rate = 0;
    uint32_t byte_rate = 0;
    uint16_t block_align = 0;
    uint16_t bits_per_sample = 0;
} __attribute__((packed));

class RiffWaveReader {
public:
    RiffWaveReader() = default;
    ~RiffWaveReader();

    RiffWaveReader(const RiffWaveReader&) = delete;
    auto operator=(const RiffWaveReader&) -> RiffWaveReader& = delete;

    // Open a WAV file. Returns false if the file is invalid or not PCM WAV.
    auto Open(const std::string& path) -> bool;

    // Close the file.
    auto Close() -> void;

    // Read the next PCM chunk. Returns bytes written to buffer, 0 = EOF.
    auto ReadChunk(uint8_t* buffer, size_t max_bytes) -> size_t;

    // Seek to the beginning of PCM data (rewind).
    auto Rewind() -> bool;

    // Queries
    [[nodiscard]] auto IsOpen() const -> bool { return fp_ != nullptr; }
    [[nodiscard]] auto GetChannels() const -> uint16_t { return channels_; }
    [[nodiscard]] auto GetSampleRate() const -> uint32_t { return sample_rate_; }
    [[nodiscard]] auto GetBitsPerSample() const -> uint16_t { return bits_per_sample_; }
    [[nodiscard]] auto GetTotalPCMBytes() const -> size_t { return data_bytes_; }

private:
    FILE* fp_ = nullptr;
    size_t data_start_ = 0;
    size_t data_bytes_ = 0;
    uint16_t channels_ = 0;
    uint32_t sample_rate_ = 0;
    uint16_t bits_per_sample_ = 0;
};

} // namespace atom

#endif // ATOM_BACKEND_BUILTIN_RIFF_WAVE_READER_HPP

/**
  * @file           : RiffWaveReader.cpp
  * @author         : Romi Brooks
  * @brief          : WAV audio file decoder implementation
  * @attention      :
  * @date           : 2026/7/8
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "RiffWaveReader.hpp"

#include <cstring>
#include <cstdio>

#include <Utilities/Utf8/Utf8.hpp>

namespace atom {

RiffWaveReader::~RiffWaveReader() {
    Close();
}

auto RiffWaveReader::Open(const std::string& path) -> bool {
    if (fp_)
        Close();

#ifdef _WIN32
    // Use _wfopen to support UTF-8 paths with non-ASCII characters
    const auto wpath = atom::Utf8ToWide(path);
    fp_ = _wfopen(wpath.c_str(), L"rb");
#else
    fp_ = std::fopen(path.c_str(), "rb");
#endif
    if (!fp_)
        return false;

    WavHeader header;
    if (std::fread(&header, 1, sizeof(WavHeader), fp_) != sizeof(WavHeader)) {
        std::fclose(fp_);
        fp_ = nullptr;
        return false;
    }

    // Validate RIFF/WAVE/fmt signatures
    if (std::memcmp(header.chunk_id, "RIFF", 4) != 0 || std::memcmp(header.format, "WAVE", 4) != 0 ||
        std::memcmp(header.subchunk_id, "fmt ", 4) != 0) {
        std::fclose(fp_);
        fp_ = nullptr;
        return false;
    }

    // Only uncompressed PCM is supported
    if (header.audio_format != 1) {
        std::fclose(fp_);
        fp_ = nullptr;
        return false;
    }

    channels_ = header.num_channels;
    sample_rate_ = header.sample_rate;
    bits_per_sample_ = header.bits_per_sample;

    // Skip any extra format bytes beyond the standard 16-byte fmt chunk
    long offset = sizeof(WavHeader);
    if (header.subchunk_size > 16) {
        std::fseek(fp_, static_cast<long>(header.subchunk_size) - 16, SEEK_CUR);
        offset += static_cast<long>(header.subchunk_size) - 16;
    }

    // Scan chunks until we find the "data" chunk
    char chunk_id[4]{};
    uint32_t chunk_size = 0;
    while (true) {
        if (std::fread(chunk_id, 1, 4, fp_) != 4) {
            std::fclose(fp_);
            fp_ = nullptr;
            return false;
        }
        if (std::fread(&chunk_size, 1, 4, fp_) != 4) {
            std::fclose(fp_);
            fp_ = nullptr;
            return false;
        }
        offset += 8;

        if (std::memcmp(chunk_id, "data", 4) == 0)
            break;

        // Skip other chunks (e.g., "LIST", "fact")
        std::fseek(fp_, static_cast<long>(chunk_size), SEEK_CUR);
        offset += static_cast<long>(chunk_size);
    }

    data_start_ = static_cast<size_t>(offset);
    data_bytes_ = chunk_size;

    // Seek to start of PCM data
    std::fseek(fp_, static_cast<long>(data_start_), SEEK_SET);
    return true;
}

auto RiffWaveReader::Close() -> void {
    if (fp_) {
        std::fclose(fp_);
        fp_ = nullptr;
    }
}

auto RiffWaveReader::ReadChunk(uint8_t* buffer, size_t max_bytes) -> size_t {
    if (!fp_)
        return 0;

    const long current_pos = std::ftell(fp_);
    const size_t bytes_read_so_far = static_cast<size_t>(current_pos) - data_start_;
    const size_t remaining = data_bytes_ - bytes_read_so_far;

    if (remaining == 0)
        return 0;

    const size_t to_read = (max_bytes < remaining) ? max_bytes : remaining;
    return std::fread(buffer, 1, to_read, fp_);
}

auto RiffWaveReader::Rewind() -> bool {
    if (!fp_)
        return false;
    std::fseek(fp_, static_cast<long>(data_start_), SEEK_SET);
    return true;
}

} // namespace atom

/**
  * @file           : RiffWaveReader.cpp
  * @author         : Romi Brooks
  * @brief          : WAV audio file decoder implementation
  * @attention      :
  * @date           : 2026/7/8
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "RiffWaveReader.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>

#include <Utilities/Utf8/Utf8.hpp>

namespace atom::backend::audio_decoder {

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

    // Only uncompressed PCM (1) and IEEE float (3, 32-bit) are supported
    if (header.audio_format != 1 && !(header.audio_format == 3 && header.bits_per_sample == 32)) {
        std::fclose(fp_);
        fp_ = nullptr;
        return false;
    }

    channels_ = header.num_channels;
    sample_rate_ = header.sample_rate;
    bits_per_sample_ = header.bits_per_sample;
    audio_format_ = header.audio_format;

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

auto RiffWaveReader::OpenFromMemory(const void* data, const std::size_t size) -> bool {
    Close();
    if (!data || size < sizeof(WavHeader))
        return false;

    const auto* bytes = static_cast<const uint8_t*>(data);

    // Validate RIFF/WAVE/fmt signatures
    WavHeader header;
    std::memcpy(&header, bytes, sizeof(WavHeader));
    if (std::memcmp(header.chunk_id, "RIFF", 4) != 0 || std::memcmp(header.format, "WAVE", 4) != 0 ||
        std::memcmp(header.subchunk_id, "fmt ", 4) != 0)
        return false;

    // Only uncompressed PCM (1) and IEEE float (3, 32-bit) are supported
    if (header.audio_format != 1 && !(header.audio_format == 3 && header.bits_per_sample == 32))
        return false;

    channels_ = header.num_channels;
    sample_rate_ = header.sample_rate;
    bits_per_sample_ = header.bits_per_sample;
    audio_format_ = header.audio_format;

    // Skip any extra format bytes beyond the standard 16-byte fmt chunk
    std::size_t offset = sizeof(WavHeader);
    if (header.subchunk_size > 16)
        offset += header.subchunk_size - 16;

    // Scan chunks until we find the "data" chunk
    while (offset + 8 <= size) {
        char chunk_id[4]{};
        uint32_t chunk_size = 0;
        std::memcpy(chunk_id, bytes + offset, 4);
        std::memcpy(&chunk_size, bytes + offset + 4, 4);
        offset += 8;

        if (std::memcmp(chunk_id, "data", 4) == 0) {
            data_start_ = offset;
            data_bytes_ = std::min<uint64_t>(chunk_size, size - offset);
            mem_data_ = bytes;
            mem_size_ = size;
            mem_pos_ = offset;
            return true;
        }

        // Skip other chunks (e.g., "LIST", "fact"); bail on truncated data
        if (chunk_size > size - offset)
            return false;
        offset += chunk_size;
    }

    return false;
}

auto RiffWaveReader::Close() -> void {
    if (fp_) {
        std::fclose(fp_);
        fp_ = nullptr;
    }
    mem_data_ = nullptr;
    mem_size_ = 0;
    mem_pos_ = 0;
}

auto RiffWaveReader::ReadChunk(uint8_t* buffer, const size_t max_bytes) -> size_t {
    if (fp_) {
        const long current_pos = std::ftell(fp_);
        const size_t bytes_read_so_far = static_cast<size_t>(current_pos) - data_start_;
        const size_t remaining = data_bytes_ - bytes_read_so_far;

        if (remaining == 0)
            return 0;

        const size_t to_read = (max_bytes < remaining) ? max_bytes : remaining;
        return std::fread(buffer, 1, to_read, fp_);
    }

    if (!mem_data_)
        return 0;

    const size_t bytes_read_so_far = mem_pos_ - data_start_;
    const size_t remaining = data_bytes_ - bytes_read_so_far;
    if (remaining == 0)
        return 0;

    const size_t to_read = (max_bytes < remaining) ? max_bytes : remaining;
    std::memcpy(buffer, mem_data_ + mem_pos_, to_read);
    mem_pos_ += to_read;
    return to_read;
}

auto RiffWaveReader::Rewind() -> bool {
    if (fp_) {
        std::fseek(fp_, static_cast<long>(data_start_), SEEK_SET);
        return true;
    }
    if (!mem_data_)
        return false;
    mem_pos_ = data_start_;
    return true;
}

} // namespace atom::backend::audio_decoder

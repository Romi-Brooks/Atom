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

namespace atom::backend::audio_decoder {
namespace {

// Reads exactly `count` bytes at `offset` from an IFile into `buffer`. Returns
// false on any short read or I/O error, so the caller can report InvalidData or
// IoError without a partial parse.
auto ReadFileAt(atom::fs::IFile& file, const uint64_t offset, void* buffer, const std::size_t count) -> bool {
    if (count == 0)
        return true;
    const std::span<std::byte> destination{static_cast<std::byte*>(buffer), count};
    return file.ReadAt(offset, destination) == atom::fs::Result::Success;
}

} // namespace

RiffWaveReader::~RiffWaveReader() {
    Close();
}

auto RiffWaveReader::OpenFromMemory(const void* data, const std::size_t size) -> atom::audio::DecoderOpenStatus {
    Close();
    if (!data || size < sizeof(WavHeader))
        return atom::audio::DecoderOpenStatus::InvalidData;

    const auto* bytes = static_cast<const uint8_t*>(data);

    // Validate RIFF/WAVE/fmt signatures
    WavHeader header;
    std::memcpy(&header, bytes, sizeof(WavHeader));
    if (std::memcmp(header.chunk_id, "RIFF", 4) != 0 || std::memcmp(header.format, "WAVE", 4) != 0 ||
        std::memcmp(header.subchunk_id, "fmt ", 4) != 0)
        return atom::audio::DecoderOpenStatus::InvalidData;

    // Only uncompressed PCM (1) and IEEE float (3, 32-bit) are supported
    if (header.audio_format != 1 && !(header.audio_format == 3 && header.bits_per_sample == 32))
        return atom::audio::DecoderOpenStatus::UnsupportedFormat;

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
            return atom::audio::DecoderOpenStatus::Opened;
        }

        // Skip other chunks (e.g., "LIST", "fact"); bail on truncated data
        if (chunk_size > size - offset)
            return atom::audio::DecoderOpenStatus::InvalidData;
        offset += chunk_size;
    }

    return atom::audio::DecoderOpenStatus::InvalidData;
}

auto RiffWaveReader::OpenStream(atom::fs::IFile& file) -> atom::audio::DecoderOpenStatus {
    Close();

    const uint64_t file_size = file.Size();
    if (file_size < sizeof(WavHeader))
        return atom::audio::DecoderOpenStatus::InvalidData;

    // Validate RIFF/WAVE/fmt signatures.
    WavHeader header;
    if (!ReadFileAt(file, 0, &header, sizeof(WavHeader)))
        return atom::audio::DecoderOpenStatus::IoError;
    if (std::memcmp(header.chunk_id, "RIFF", 4) != 0 || std::memcmp(header.format, "WAVE", 4) != 0 ||
        std::memcmp(header.subchunk_id, "fmt ", 4) != 0)
        return atom::audio::DecoderOpenStatus::InvalidData;

    // Only uncompressed PCM (1) and IEEE float (3, 32-bit) are supported.
    if (header.audio_format != 1 && !(header.audio_format == 3 && header.bits_per_sample == 32))
        return atom::audio::DecoderOpenStatus::UnsupportedFormat;

    channels_ = header.num_channels;
    sample_rate_ = header.sample_rate;
    bits_per_sample_ = header.bits_per_sample;
    audio_format_ = header.audio_format;

    // Skip any extra format bytes beyond the standard 16-byte fmt chunk.
    uint64_t offset = sizeof(WavHeader);
    if (header.subchunk_size > 16)
        offset += header.subchunk_size - 16;

    // Scan chunks until we find the "data" chunk.
    while (offset + 8 <= file_size) {
        char chunk_id[4]{};
        uint32_t chunk_size = 0;
        if (!ReadFileAt(file, offset, chunk_id, 4) || !ReadFileAt(file, offset + 4, &chunk_size, 4))
            return atom::audio::DecoderOpenStatus::IoError;
        offset += 8;

        if (std::memcmp(chunk_id, "data", 4) == 0) {
            data_start_ = static_cast<size_t>(offset);
            data_bytes_ = static_cast<size_t>(std::min<uint64_t>(chunk_size, file_size - offset));
            file_ = &file;
            file_size_ = file_size;
            file_pos_ = offset;
            return atom::audio::DecoderOpenStatus::Opened;
        }

        // Skip other chunks (e.g., "LIST", "fact"); bail on truncated data.
        if (chunk_size > file_size - offset)
            return atom::audio::DecoderOpenStatus::InvalidData;
        offset += chunk_size;
    }

    return atom::audio::DecoderOpenStatus::InvalidData;
}

auto RiffWaveReader::Close() -> void {
    mem_data_ = nullptr;
    mem_size_ = 0;
    mem_pos_ = 0;
    file_ = nullptr;
    file_pos_ = 0;
    file_size_ = 0;
}

auto RiffWaveReader::ReadChunk(uint8_t* buffer, const size_t max_bytes) -> size_t {
    if (file_) {
        const uint64_t bytes_read_so_far = file_pos_ - data_start_;
        const uint64_t remaining = data_bytes_ - bytes_read_so_far;
        if (remaining == 0)
            return 0;

        const size_t to_read = static_cast<size_t>(std::min<uint64_t>(max_bytes, remaining));
        const std::span<std::byte> destination{reinterpret_cast<std::byte*>(buffer), to_read};
        if (file_->ReadAt(file_pos_, destination) != atom::fs::Result::Success)
            return 0;
        file_pos_ += to_read;
        return to_read;
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
    if (file_) {
        file_pos_ = data_start_;
        return true;
    }
    if (!mem_data_)
        return false;
    mem_pos_ = data_start_;
    return true;
}

auto RiffWaveReader::SeekToByte(const std::size_t byte_offset) -> bool {
    if (channels_ == 0 || bits_per_sample_ == 0)
        return false;
    const auto bytes_per_frame = static_cast<std::size_t>(channels_) * (bits_per_sample_ / 8u);
    if (bytes_per_frame == 0)
        return false;

    // Clamp to the PCM data and drop a partial frame: decoding must always
    // restart on a frame boundary.
    auto clamped = std::min(byte_offset, data_bytes_);
    clamped -= clamped % bytes_per_frame;

    if (file_) {
        file_pos_ = data_start_ + clamped;
        return true;
    }
    if (!mem_data_)
        return false;
    mem_pos_ = data_start_ + clamped;
    return true;
}

} // namespace atom::backend::audio_decoder

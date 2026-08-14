/**
  * @file           : SDL3WavDecoder.cpp
  * @brief          : Incremental WAV decoder backed by SDL_IOStream
**/

#include "SDL3WavDecoder.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

#include <SDL3/SDL.h>

#include <Log/LogSystem.hpp>

namespace atom {
namespace {

auto ReadExact(SDL_IOStream* io, void* destination, const std::size_t size) -> bool {
    return SDL_ReadIO(io, destination, size) == size;
}

auto ReadU16LE(SDL_IOStream* io, std::uint16_t& value) -> bool {
    std::array<std::uint8_t, 2> bytes{};
    if (!ReadExact(io, bytes.data(), bytes.size()))
        return false;
    value = static_cast<std::uint16_t>(bytes[0]) | (static_cast<std::uint16_t>(bytes[1]) << 8u);
    return true;
}

auto ReadU32LE(SDL_IOStream* io, std::uint32_t& value) -> bool {
    std::array<std::uint8_t, 4> bytes{};
    if (!ReadExact(io, bytes.data(), bytes.size()))
        return false;
    value = static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8u) |
            (static_cast<std::uint32_t>(bytes[2]) << 16u) | (static_cast<std::uint32_t>(bytes[3]) << 24u);
    return true;
}

} // namespace

SDL3WavDecoder::~SDL3WavDecoder() {
    Close();
}

auto SDL3WavDecoder::Open(const std::string& path) -> bool {
    Close();
    io_ = SDL_IOFromFile(path.c_str(), "rb");
    if (!io_) {
        LOG_ERROR(LogChannel::SDL_BACKEND_AUDIO, "SDL_IOFromFile failed: " + std::string(SDL_GetError()));
        return false;
    }

    std::array<char, 4> id{};
    std::uint32_t riff_size = 0;
    if (!ReadExact(io_, id.data(), id.size()) || std::memcmp(id.data(), "RIFF", 4) != 0 || !ReadU32LE(io_, riff_size) ||
        !ReadExact(io_, id.data(), id.size()) || std::memcmp(id.data(), "WAVE", 4) != 0) {
        Close();
        return false;
    }

    bool have_format = false;
    bool have_data = false;
    while (!have_data) {
        std::uint32_t chunk_size = 0;
        if (!ReadExact(io_, id.data(), id.size()) || !ReadU32LE(io_, chunk_size))
            break;
        const auto chunk_start = SDL_TellIO(io_);
        if (chunk_start < 0)
            break;

        if (std::memcmp(id.data(), "fmt ", 4) == 0 && chunk_size >= 16) {
            std::uint16_t encoding = 0;
            std::uint16_t channels = 0;
            std::uint32_t sample_rate = 0;
            std::uint32_t byte_rate = 0;
            std::uint16_t block_align = 0;
            std::uint16_t bits = 0;
            if (!ReadU16LE(io_, encoding) || !ReadU16LE(io_, channels) || !ReadU32LE(io_, sample_rate) ||
                !ReadU32LE(io_, byte_rate) || !ReadU16LE(io_, block_align) || !ReadU16LE(io_, bits) ||
                (encoding != 1 && encoding != 3) || (encoding == 3 && bits != 32))
                break;
            info_.sample_rate = sample_rate;
            info_.channels = channels;
            source_bits_per_sample_ = bits;
            // SDL has no packed 24-bit audio format. Expose S32 and expand
            // each packed sample while decoding.
            info_.bits_per_sample = bits == 24 ? 32 : bits;
            info_.is_float = encoding == 3;
            have_format = channels > 0 && sample_rate > 0 && bits > 0 && bits % 8 == 0;
        } else if (std::memcmp(id.data(), "data", 4) == 0) {
            if (!have_format)
                break;
            data_offset_ = chunk_start;
            data_size_ = chunk_size;
            have_data = true;
            break;
        }

        const auto next = chunk_start + static_cast<Sint64>(chunk_size) + (chunk_size & 1u);
        if (SDL_SeekIO(io_, next, SDL_IO_SEEK_SET) < 0)
            break;
    }

    const auto bytes_per_frame = static_cast<std::uint64_t>(info_.channels) * (source_bits_per_sample_ / 8u);
    if (!have_data || bytes_per_frame == 0 || SDL_SeekIO(io_, data_offset_, SDL_IO_SEEK_SET) < 0) {
        Close();
        return false;
    }
    info_.total_pcm_frames = data_size_ / bytes_per_frame;
    bytes_read_ = 0;
    return true;
}

auto SDL3WavDecoder::Close() -> void {
    if (io_)
        SDL_CloseIO(io_);
    io_ = nullptr;
    data_offset_ = 0;
    data_size_ = 0;
    bytes_read_ = 0;
    decode_scratch_.clear();
    info_ = {};
    source_bits_per_sample_ = 0;
}

auto SDL3WavDecoder::DecodeChunk(uint8_t* output, const uint32_t max_bytes) -> uint32_t {
    if (!io_ || !output || max_bytes == 0 || bytes_read_ >= data_size_)
        return 0;

    if (source_bits_per_sample_ == 24) {
        const auto input_frame_bytes = static_cast<std::size_t>(info_.channels) * 3u;
        const auto output_frame_bytes = static_cast<std::size_t>(info_.channels) * 4u;
        if (input_frame_bytes == 0 || max_bytes < output_frame_bytes)
            return 0;

        const auto remaining_frames = (data_size_ - bytes_read_) / input_frame_bytes;
        const auto requested_frames = static_cast<std::uint64_t>(max_bytes / output_frame_bytes);
        const auto frames = (std::min)(remaining_frames, requested_frames);
        const auto input_bytes = static_cast<std::size_t>(frames) * input_frame_bytes;
        decode_scratch_.resize(input_bytes);

        const auto decoded = SDL_ReadIO(io_, decode_scratch_.data(), input_bytes);
        bytes_read_ += decoded;
        const auto samples = (decoded / input_frame_bytes) * info_.channels;
        for (std::size_t sample_index = 0; sample_index < samples; ++sample_index) {
            const auto src = sample_index * 3u;
            const auto dst = sample_index * 4u;
            // Scale packed signed 24-bit PCM to SDL's signed 32-bit range.
            // In little-endian form this is simply 00, low, mid, high.
            output[dst] = 0;
            output[dst + 1] = decode_scratch_[src];
            output[dst + 2] = decode_scratch_[src + 1];
            output[dst + 3] = decode_scratch_[src + 2];
        }
        if (decoded == 0 && SDL_GetIOStatus(io_) == SDL_IO_STATUS_ERROR) {
            LOG_ERROR(LogChannel::SDL_BACKEND_AUDIO, "WAV stream read failed: " + std::string(SDL_GetError()));
            Close();
        }
        return static_cast<uint32_t>(samples * 4u);
    }

    const auto remaining = data_size_ - bytes_read_;
    const auto requested = static_cast<std::size_t>((std::min)(remaining, static_cast<std::uint64_t>(max_bytes)));
    const auto decoded = SDL_ReadIO(io_, output, requested);
    bytes_read_ += decoded;
    if (decoded == 0 && SDL_GetIOStatus(io_) == SDL_IO_STATUS_ERROR) {
        LOG_ERROR(LogChannel::SDL_BACKEND_AUDIO, "WAV stream read failed: " + std::string(SDL_GetError()));
        Close(); // lets the source distinguish this from a normal EOF
    }
    return static_cast<uint32_t>(decoded);
}

auto SDL3WavDecoder::Rewind() -> bool {
    if (!io_ || SDL_SeekIO(io_, data_offset_, SDL_IO_SEEK_SET) < 0)
        return false;
    bytes_read_ = 0;
    return true;
}

auto SDL3WavDecoder::GetInfo() const -> const DecoderInfo& {
    return info_;
}
auto SDL3WavDecoder::IsOpen() const -> bool {
    return io_ != nullptr;
}

} // namespace atom

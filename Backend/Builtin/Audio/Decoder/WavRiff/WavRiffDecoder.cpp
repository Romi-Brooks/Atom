/**
  * @file           : WavRiffDecoder.cpp
  * @author         : Romi Brooks
  * @brief          : The engine's single WAV decoder, backed by RiffWaveReader
  * @attention      : Keeps the file open and reads PCM in bounded chunks,
  *                   matching the engine's streaming decoder contract.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "WavRiffDecoder.hpp"

#include <cstdint>

namespace atom {

auto WavRiffDecoder::Open(const std::string& path) -> bool {
    Close();
    if (!reader_.Open(path))
        return false;

    info_.sample_rate = reader_.GetSampleRate();
    info_.channels = reader_.GetChannels();
    info_.bits_per_sample = reader_.GetBitsPerSample();
    info_.is_float = reader_.GetAudioFormat() == 3;
    source_bits_per_sample_ = info_.bits_per_sample;
    // 24-bit packed PCM has no native SDL format: expose S32 and expand each
    // packed sample while decoding.
    if (source_bits_per_sample_ == 24)
        info_.bits_per_sample = 32;

    const auto bytes_per_frame = static_cast<uint64_t>(info_.channels) * (source_bits_per_sample_ / 8u);
    info_.total_pcm_frames = bytes_per_frame == 0 ? 0 : reader_.GetTotalPCMBytes() / bytes_per_frame;
    return info_.total_pcm_frames > 0;
}

auto WavRiffDecoder::Close() -> void {
    reader_.Close();
    decode_scratch_.clear();
    source_bits_per_sample_ = 0;
    info_ = {};
}

auto WavRiffDecoder::DecodeChunk(uint8_t* output, const uint32_t max_bytes) -> uint32_t {
    if (!reader_.IsOpen() || !output || max_bytes == 0)
        return 0;

    if (source_bits_per_sample_ == 24) {
        const auto input_frame_bytes = static_cast<std::size_t>(info_.channels) * 3u;
        const auto output_frame_bytes = static_cast<std::size_t>(info_.channels) * 4u;
        if (input_frame_bytes == 0 || max_bytes < output_frame_bytes)
            return 0;

        const auto frames = static_cast<std::size_t>(max_bytes / output_frame_bytes);
        const auto input_bytes = frames * input_frame_bytes;
        decode_scratch_.resize(input_bytes);

        const auto decoded = reader_.ReadChunk(decode_scratch_.data(), input_bytes);
        const auto samples = (decoded / input_frame_bytes) * info_.channels;
        for (std::size_t sample_index = 0; sample_index < samples; ++sample_index) {
            const auto src = sample_index * 3u;
            const auto dst = sample_index * 4u;
            // Scale packed signed 24-bit PCM to signed 32-bit range.
            // In little-endian form this is simply 00, low, mid, high.
            output[dst] = 0;
            output[dst + 1] = decode_scratch_[src];
            output[dst + 2] = decode_scratch_[src + 1];
            output[dst + 3] = decode_scratch_[src + 2];
        }
        return static_cast<uint32_t>(samples * 4u);
    }

    return static_cast<uint32_t>(reader_.ReadChunk(output, max_bytes));
}

auto WavRiffDecoder::Rewind() -> bool {
    return reader_.Rewind();
}

auto WavRiffDecoder::GetInfo() const -> const DecoderInfo& {
    return info_;
}

auto WavRiffDecoder::IsOpen() const -> bool {
    return reader_.IsOpen();
}

} // namespace atom

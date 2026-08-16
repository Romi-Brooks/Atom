/**
  * @file           : WavProfDecoder.cpp
  * @author         : Romi Brooks
  * @brief          : The engine's single WAV decoder (WavProf), backed by RiffWaveReader
  * @attention      : Keeps the file open and reads PCM in bounded chunks,
  *                   matching the engine's streaming decoder contract.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "WavProfDecoder.hpp"

#include <cstdint>

#include <Log/LogSystem.hpp>

namespace atom {

auto WavProfDecoder::Open(const std::string& path) -> bool {
    Close();
    if (!reader_.Open(path)) {
        LOG_ERROR(atom::audio::LogChannel::WAVPROF, "WavProf: failed to open WAV file: " + path);
        return false;
    }
    return SetupInfo(path);
}

auto WavProfDecoder::OpenFromMemory(const void* data, const std::size_t size) -> bool {
    Close();
    if (!reader_.OpenFromMemory(data, size)) {
        LOG_ERROR(atom::audio::LogChannel::WAVPROF, "WavProf: failed to open WAV from memory buffer");
        return false;
    }
    return SetupInfo("(memory)");
}

auto WavProfDecoder::SetupInfo(const std::string& source_label) -> bool {
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
    if (info_.total_pcm_frames == 0) {
        LOG_ERROR(atom::audio::LogChannel::WAVPROF, "WavProf: WAV stream has no PCM data: " + source_label);
        return false;
    }

    LOG_DEBUG(atom::audio::LogChannel::WAVPROF,
              "WavProf: WAV stream opened: " + source_label + " (sample_rate=" + std::to_string(info_.sample_rate) +
                  ", channels=" + std::to_string(info_.channels) + ", bits_per_sample=" +
                  std::to_string(info_.bits_per_sample) + ")");
    return true;
}

auto WavProfDecoder::Close() -> void {
    reader_.Close();
    decode_scratch_.clear();
    source_bits_per_sample_ = 0;
    info_ = {};
}

auto WavProfDecoder::DecodeChunk(uint8_t* output, const uint32_t max_bytes) -> uint32_t {
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

auto WavProfDecoder::Rewind() -> bool {
    return reader_.Rewind();
}

auto WavProfDecoder::GetInfo() const -> const DecoderInfo& {
    return info_;
}

auto WavProfDecoder::IsOpen() const -> bool {
    return reader_.IsOpen();
}

} // namespace atom

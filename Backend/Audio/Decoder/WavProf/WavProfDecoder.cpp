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
#include <limits>

#include <Log/LogSystem.hpp>

namespace atom::backend::audio_decoder {

auto WavProfDecoder::OpenFromMemory(const void* data, const std::size_t size) -> atom::audio::DecoderOpenStatus {
    Close();
    const auto status = reader_.OpenFromMemory(data, size);
    if (status != atom::audio::DecoderOpenStatus::Opened) {
        LOG_DEBUG(atom::log::audio::WavProf,
                  std::string{"WavProf: declined memory buffer ("} + atom::audio::DescribeDecoderOpenStatus(status) +
                      ")");
        return status;
    }
    return SetupInfo("(memory)") ? atom::audio::DecoderOpenStatus::Opened
                                 : atom::audio::DecoderOpenStatus::InvalidData;
}

auto WavProfDecoder::OpenStream(atom::fs::IFile& file) -> atom::audio::DecoderOpenStatus {
    Close();
    const auto status = reader_.OpenStream(file);
    if (status != atom::audio::DecoderOpenStatus::Opened) {
        LOG_DEBUG(atom::log::audio::WavProf,
                  std::string{"WavProf: declined stream ("} + atom::audio::DescribeDecoderOpenStatus(status) + ")");
        return status;
    }
    return SetupInfo("(vfs)") ? atom::audio::DecoderOpenStatus::Opened
                              : atom::audio::DecoderOpenStatus::InvalidData;
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
        LOG_DEBUG(atom::log::audio::WavProf, "WavProf: WAV stream has no PCM data: " + source_label);
        return false;
    }

    LOG_DEBUG(atom::log::audio::WavProf, "WavProf: WAV stream opened: " + source_label +
                                                    " (sample_rate=" + std::to_string(info_.sample_rate) +
                                                    ", channels=" + std::to_string(info_.channels) +
                                                    ", bits_per_sample=" + std::to_string(info_.bits_per_sample) + ")");
    return true;
}

auto WavProfDecoder::Close() -> void {
    reader_.Close();
    source_bits_per_sample_ = 0;
    info_ = {};
}

auto WavProfDecoder::DecodeChunk(uint8_t* output, const uint32_t max_bytes) -> uint32_t {
    if (!reader_.IsOpen() || !output || max_bytes == 0)
        return 0;

    if (source_bits_per_sample_ == 24) {
        // 24-bit packed PCM has no SDL format, so it is widened to S32 here: three
        // bytes in, four bytes out, per *sample* (interleaved channel value), which
        // is what SDL's own PCM_ConvertSint24ToSint32 does as well.
        //
        // The packed read goes into the tail of the caller's buffer (one byte of
        // slack per sample) and the widening runs in place from the front: the
        // write window of sample s always ends below the first not-yet-read packed
        // byte, so no scratch buffer and no second full copy are needed.
        const auto capacity_samples = static_cast<std::size_t>(max_bytes) / 4u;
        if (capacity_samples == 0)
            return 0;

        auto* packed = output + capacity_samples;
        const auto decoded = reader_.ReadChunk(packed, capacity_samples * 3u);
        const auto read_samples = decoded / 3u;
        for (std::size_t index = 0; index < read_samples; ++index) {
            const auto src = index * 3u;
            const auto dst = index * 4u;
            // Scale packed signed 24-bit PCM to signed 32-bit range.
            // In little-endian form this is simply 00, low, mid, high.
            output[dst] = 0;
            output[dst + 1] = packed[src];
            output[dst + 2] = packed[src + 1];
            output[dst + 3] = packed[src + 2];
        }
        return static_cast<uint32_t>(read_samples * 4u);
    }

    return static_cast<uint32_t>(reader_.ReadChunk(output, max_bytes));
}

auto WavProfDecoder::Rewind() -> bool {
    return reader_.Rewind();
}

auto WavProfDecoder::IsSeekable() const -> bool {
    // Uncompressed PCM: a seek is a file offset (or an in-memory cursor) move.
    return reader_.IsOpen();
}

auto WavProfDecoder::SeekToFrame(const std::uint64_t frame) -> bool {
    if (!reader_.IsOpen() || info_.channels == 0 || source_bits_per_sample_ == 0)
        return false;

    // The reader stores packed source PCM, so the frame stride uses the *source*
    // width (a 24-bit file is widened to S32 only while decoding).
    const auto bytes_per_frame = static_cast<std::uint64_t>(info_.channels) * (source_bits_per_sample_ / 8u);
    if (bytes_per_frame == 0)
        return false;

    const auto byte_offset = frame * bytes_per_frame;
    if (byte_offset > std::numeric_limits<std::size_t>::max())
        return false;
    return reader_.SeekToByte(static_cast<std::size_t>(byte_offset));
}

auto WavProfDecoder::GetInfo() const -> const atom::audio::DecoderInfo& {
    return info_;
}

auto WavProfDecoder::IsOpen() const -> bool {
    return reader_.IsOpen();
}

auto CreateWavProfDecoder() -> std::unique_ptr<atom::audio::IAudioDecoder> {
    return std::make_unique<WavProfDecoder>();
}

} // namespace atom::backend::audio_decoder

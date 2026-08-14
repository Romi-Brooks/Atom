#include "BuiltinWavDecoder.hpp"

namespace atom {

auto BuiltinWavDecoder::Open(const std::string& path) -> bool {
    Close();
    if (!reader_.Open(path))
        return false;

    info_.sample_rate = reader_.GetSampleRate();
    info_.channels = reader_.GetChannels();
    info_.bits_per_sample = reader_.GetBitsPerSample();
    info_.is_float = false;
    const auto bytes_per_frame = static_cast<uint64_t>(info_.channels) * (info_.bits_per_sample / 8u);
    info_.total_pcm_frames = bytes_per_frame == 0 ? 0 : reader_.GetTotalPCMBytes() / bytes_per_frame;
    return info_.total_pcm_frames > 0;
}

auto BuiltinWavDecoder::Close() -> void {
    reader_.Close();
    info_ = {};
}

auto BuiltinWavDecoder::DecodeChunk(uint8_t* output, const uint32_t max_bytes) -> uint32_t {
    return static_cast<uint32_t>(reader_.ReadChunk(output, max_bytes));
}

auto BuiltinWavDecoder::Rewind() -> bool {
    return reader_.Rewind();
}

auto BuiltinWavDecoder::GetInfo() const -> const DecoderInfo& {
    return info_;
}

auto BuiltinWavDecoder::IsOpen() const -> bool {
    return reader_.IsOpen();
}

} // namespace atom

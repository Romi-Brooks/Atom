/**
  * @file           : AtomWavDecoderBackend.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2026/7/5
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "AtomWavDecoderBackend.hpp"
#include <WavDecoder.hpp>

namespace atom {

AtomWavDecoderBackend::AtomWavDecoderBackend()
    : decoder_(new WavDecoder())
{
}

AtomWavDecoderBackend::~AtomWavDecoderBackend() {
    delete decoder_;
}

auto AtomWavDecoderBackend::Open(const std::string& path) -> bool {
    if (!decoder_->Open(path)) return false;

    info_.sample_rate = decoder_->GetSampleRate();
    info_.channels = decoder_->GetChannels();
    info_.bits_per_sample = decoder_->GetBitsPerSample();
    info_.total_pcm_frames = decoder_->GetTotalPCMBytes() /
        (decoder_->GetBitsPerSample() / 8 * decoder_->GetChannels());

    return true;
}

auto AtomWavDecoderBackend::Close() -> void {
    decoder_->Close();
    info_ = DecoderInfo{};
}

auto AtomWavDecoderBackend::DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t {
    return static_cast<uint32_t>(decoder_->ReadChunk(output, max_bytes));
}

auto AtomWavDecoderBackend::Rewind() -> bool {
    return decoder_->Rewind();
}

auto AtomWavDecoderBackend::GetInfo() const -> const DecoderInfo& {
    return info_;
}

auto AtomWavDecoderBackend::IsOpen() const -> bool {
    return decoder_->IsOpen();
}

} // namespace atom

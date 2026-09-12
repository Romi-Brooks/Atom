/**
  * @file           : WavProfDecoder.hpp
  * @author         : Romi Brooks
  * @brief          : The engine's single WAV decoder (WavProf), backed by RiffWaveReader
  * @attention      : Handles uncompressed PCM (8/16/24/32-bit) and IEEE float
  *                   (32-bit). 24-bit packed PCM is expanded to S32 because SDL
  *                   has no native packed 24-bit format.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_WAVPROF_DECODER_HPP
#define ATOM_BACKEND_WAVPROF_DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Backend/Audio/Decoder/WavProf/RiffWaveReader.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom::backend::audio_decoder {
class WavProfDecoder final : public atom::audio::IAudioDecoder {
    public:
        [[nodiscard]] auto Open(const std::string& path) -> atom::audio::DecoderOpenStatus override;
        [[nodiscard]] auto OpenFromMemory(const void* data, std::size_t size) -> atom::audio::DecoderOpenStatus override;
        auto Close() -> void override;
        auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t override;
        auto Rewind() -> bool override;
        [[nodiscard]] auto IsSeekable() const -> bool override;
        auto SeekToFrame(std::uint64_t frame) -> bool override;
        [[nodiscard]] auto GetInfo() const -> const atom::audio::DecoderInfo& override;
        [[nodiscard]] auto IsOpen() const -> bool override;

    private:
        // Fills info_ from the reader; returns false when the stream carries no
        // PCM data. source_label is only used for logging.
        auto SetupInfo(const std::string& source_label) -> bool;

        RiffWaveReader reader_;
        atom::audio::DecoderInfo info_{};
        uint16_t source_bits_per_sample_ = 0;
};

// Registry factory for AudioDecoderRegistry::Register/Replace.
[[nodiscard]] auto CreateWavProfDecoder() -> std::unique_ptr<atom::audio::IAudioDecoder>;

} // namespace atom::backend::audio_decoder

#endif // ATOM_BACKEND_WAVPROF_DECODER_HPP

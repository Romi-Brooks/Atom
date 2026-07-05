/**
  * @file           : AtomWavDecoderBackend.hpp
  * @author         : Romi Brooks
  * @brief          : IAudioDecoder implementation wrapping the Atom WavDecoder
  * @attention      : Alternative to SDL3's WAV decoder (SDL_LoadWAV).
  *                   Uses the self-contained WavDecoder from ThirdParty.
  * @date           : 2026/7/5
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ATOM_WAV_DECODER_BACKEND_HPP
#define ATOM_ATOM_WAV_DECODER_BACKEND_HPP

#include <Engine/Interfaces/IAudioDecoder.hpp>
#include <WavDecoder.hpp>

class AtomWavDecoderBackend : public IAudioDecoder {
public:
    AtomWavDecoderBackend();
    ~AtomWavDecoderBackend() override;

    AtomWavDecoderBackend(const AtomWavDecoderBackend&) = delete;
    auto operator=(const AtomWavDecoderBackend&) -> AtomWavDecoderBackend& = delete;

    // IAudioDecoder
    auto Open(const std::string& path) -> bool override;
    auto Close() -> void override;
    auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t override;
    auto Rewind() -> bool override;
    [[nodiscard]] auto GetInfo() const -> const DecoderInfo& override;
    [[nodiscard]] auto IsOpen() const -> bool override;

private:
    WavDecoder* decoder_ = nullptr;
    DecoderInfo info_{};
};

} // namespace atom

#endif // ATOM_ATOM_WAV_DECODER_BACKEND_HPP

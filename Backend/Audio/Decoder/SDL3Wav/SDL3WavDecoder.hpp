/**
  * @file           : SDL3WavDecoder.hpp
  * @author         : Romi Brooks
  * @brief          : WAV decoder backed by SDL3's own loader (SDL_LoadWAV)
  * @attention      : SDL3 decodes the whole file up front: PCM U8/S16/S32,
  *                   IEEE float, MS ADPCM, IMA ADPCM, A-Law and mu-Law, all
  *                   widened by SDL into one resident buffer. DecodeChunk() then
  *                   only copies out of it and seeking is an index into it.
  *                   Prefer WavProfDecoder when a long file must be streamed
  *                   with bounded memory; prefer this decoder for format
  *                   coverage (the compressed WAV encodings WavProf cannot read)
  *                   and for short assets that are resident anyway.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_AUDIO_SDL3_WAV_DECODER_HPP
#define ATOM_BACKEND_AUDIO_SDL3_WAV_DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom::backend::audio_decoder {

class SDL3WavDecoder final : public atom::audio::IAudioDecoder {
    public:
        SDL3WavDecoder();
        ~SDL3WavDecoder() override;

        SDL3WavDecoder(const SDL3WavDecoder&) = delete;
        auto operator=(const SDL3WavDecoder&) -> SDL3WavDecoder& = delete;

        // atom::audio::IAudioDecoder
        [[nodiscard]] auto OpenFromMemory(const void* data, std::size_t size) -> atom::audio::DecoderOpenStatus override;
        [[nodiscard]] auto OpenStream(atom::fs::IFile& file) -> atom::audio::DecoderOpenStatus override;
        auto Close() -> void override;
        auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t override;
        auto Rewind() -> bool override;
        [[nodiscard]] auto IsSeekable() const -> bool override;
        auto SeekToFrame(std::uint64_t frame) -> bool override;
        [[nodiscard]] auto GetInfo() const -> const atom::audio::DecoderInfo& override;
        [[nodiscard]] auto IsOpen() const -> bool override;

    private:
        // Holds the SDL buffer/spec/cursor; defined in the .cpp so this header
        // never leaks SDL types to its consumers.
        struct Impl;

        auto AdoptLoadedBuffer(const std::string& source_label) -> bool;
        std::unique_ptr<Impl> impl_;
        atom::audio::DecoderInfo info_{};
};

// Registry factory for AudioDecoderRegistry::Register/Replace.
[[nodiscard]] auto CreateSDL3WavDecoder() -> std::unique_ptr<atom::audio::IAudioDecoder>;

} // namespace atom::backend::audio_decoder

#endif // ATOM_BACKEND_AUDIO_SDL3_WAV_DECODER_HPP

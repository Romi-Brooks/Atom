/**
  * @file           : Minimp3Decoder.hpp
  * @author         : Romi Brooks
  * @brief          : Streaming MP3 decoder backed by minimp3 (lieff/minimp3)
  * @attention      : Uses the mp3dec_ex callback-I/O API to keep the file open
  *                   and decode PCM in bounded chunks, matching the engine's
  *                   streaming decoder contract. This is the default decoder
  *                   for the ".mp3" extension in every decoder backend.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_BUILTIN_MINIMP3_DECODER_HPP
#define ATOM_BACKEND_BUILTIN_MINIMP3_DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom {

class Minimp3Decoder final : public IAudioDecoder {
public:
    Minimp3Decoder();
    ~Minimp3Decoder() override;

    Minimp3Decoder(const Minimp3Decoder&) = delete;
    auto operator=(const Minimp3Decoder&) -> Minimp3Decoder& = delete;

    // IAudioDecoder
    auto Open(const std::string& path) -> bool override;
    auto OpenFromMemory(const void* data, std::size_t size) -> bool override;
    auto Close() -> void override;
    auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t override;
    auto Rewind() -> bool override;
    [[nodiscard]] auto GetInfo() const -> const atom::DecoderInfo& override;
    [[nodiscard]] auto IsOpen() const -> bool override;

private:
    // Holds the C mp3dec_ex_t state; defined in the .cpp so this header never
    // leaks minimp3's C headers to its consumers.
    struct Impl;

    std::unique_ptr<Impl> impl_;
    std::vector<uint8_t> scratch_;
    atom::DecoderInfo info_{};
    std::string current_path_;
};

} // namespace atom

#endif // ATOM_BACKEND_BUILTIN_MINIMP3_DECODER_HPP

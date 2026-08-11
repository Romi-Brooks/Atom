#ifndef ATOM_BACKEND_BUILTIN_WAV_DECODER_HPP
#define ATOM_BACKEND_BUILTIN_WAV_DECODER_HPP

#include <Backend/Builtin/Audio/Decoder/WavRiff/RiffWaveReader.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom {

class BuiltinWavDecoder final : public IAudioDecoder {
public:
    auto Open(const std::string& path) -> bool override;
    auto Close() -> void override;
    auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t override;
    auto Rewind() -> bool override;
    [[nodiscard]] auto GetInfo() const -> const DecoderInfo& override;
    [[nodiscard]] auto IsOpen() const -> bool override;

private:
    RiffWaveReader reader_;
    DecoderInfo info_{};
};

} // namespace atom

#endif // ATOM_BACKEND_BUILTIN_WAV_DECODER_HPP

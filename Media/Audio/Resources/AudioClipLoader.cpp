#include "AudioClipLoader.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Registry/AudioDecoderRegistry.hpp>

namespace atom {
namespace {

auto ToSampleFormat(const DecoderInfo& info) -> std::optional<AudioSampleFormat> {
    if (info.is_float && info.bits_per_sample == 32)
        return AudioSampleFormat::Float32;
    switch (info.bits_per_sample) {
    case 8:
        return AudioSampleFormat::Unsigned8;
    case 16:
        return AudioSampleFormat::Signed16;
    case 24:
    case 32:
        return AudioSampleFormat::Signed32;
    default:
        return std::nullopt;
    }
}

auto Expand24To32(const std::vector<uint8_t>& packed) -> std::vector<uint8_t> {
    std::vector<uint8_t> expanded((packed.size() / 3u) * 4u);
    for (std::size_t src = 0, dst = 0; src + 2 < packed.size(); src += 3, dst += 4) {
        int32_t sample = static_cast<int32_t>(packed[src]) | (static_cast<int32_t>(packed[src + 1]) << 8) |
                         (static_cast<int32_t>(packed[src + 2]) << 16);
        if ((sample & 0x00800000) != 0)
            sample |= static_cast<int32_t>(0xFF000000);
        expanded[dst] = static_cast<uint8_t>(sample & 0xFF);
        expanded[dst + 1] = static_cast<uint8_t>((sample >> 8) & 0xFF);
        expanded[dst + 2] = static_cast<uint8_t>((sample >> 16) & 0xFF);
        expanded[dst + 3] = static_cast<uint8_t>((sample >> 24) & 0xFF);
    }
    return expanded;
}

} // namespace

auto AudioClipLoader::Load(const std::string& path) const -> std::optional<DecodedAudio> {
    auto decoder = decoders_.CreateForFile(path);
    if (!decoder || !decoder->Open(path))
        return std::nullopt;

    const auto info = decoder->GetInfo();
    const auto format = ToSampleFormat(info);
    if (!format || info.sample_rate == 0 || info.channels == 0)
        return std::nullopt;

    std::vector<uint8_t> pcm;
    std::array<uint8_t, 64 * 1024> chunk{};
    while (const auto decoded = decoder->DecodeChunk(chunk.data(), static_cast<uint32_t>(chunk.size()))) {
        pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + decoded);
    }
    decoder->Close();
    if (pcm.empty())
        return std::nullopt;

    if (info.bits_per_sample == 24)
        pcm = Expand24To32(pcm);
    return DecodedAudio{
        .pcm = std::move(pcm),
        .spec = AudioSpec{*format, info.sample_rate, info.channels},
    };
}

auto AudioClipLoader::OpenStreaming(const std::string& path) const -> std::optional<StreamingResult> {
    auto decoder = decoders_.CreateForFile(path);
    if (!decoder || !decoder->Open(path))
        return std::nullopt;

    const auto& info = decoder->GetInfo();
    const auto format = ToSampleFormat(info);
    if (!format || info.sample_rate == 0 || info.channels == 0) {
        decoder->Close();
        return std::nullopt;
    }

    return StreamingResult{
        .decoder = std::move(decoder),
        .spec = AudioSpec{*format, info.sample_rate, info.channels},
    };
}

} // namespace atom

#ifndef ATOM_BACKEND_AUDIO_TYPES_HPP
#define ATOM_BACKEND_AUDIO_TYPES_HPP

#include <cstdint>
#include <vector>

namespace atom {

enum class AudioSampleFormat {
    Unsigned8,
    Signed16,
    Signed32,
    Float32,
};

struct AudioSpec {
    AudioSampleFormat format = AudioSampleFormat::Signed16;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
};

struct DecodedAudio {
    std::vector<uint8_t> pcm;
    AudioSpec spec;
};

[[nodiscard]] constexpr auto BytesPerSample(const AudioSampleFormat format) -> uint32_t {
    switch (format) {
    case AudioSampleFormat::Unsigned8:
        return 1;
    case AudioSampleFormat::Signed16:
        return 2;
    case AudioSampleFormat::Signed32:
    case AudioSampleFormat::Float32:
        return 4;
    }
    return 0;
}

} // namespace atom

#endif // ATOM_BACKEND_AUDIO_TYPES_HPP

#ifndef ATOM_IAUDIO_BUFFER_HPP
#define ATOM_IAUDIO_BUFFER_HPP

#include <cstdint>
#include <string>

namespace atom {

class IAudioBuffer {
public:
    virtual ~IAudioBuffer() = default;

    virtual auto LoadFromFile(const std::string& path) -> bool = 0;
    virtual auto LoadFromMemory(const uint8_t* data, uint32_t size) -> bool = 0;

    [[nodiscard]] virtual auto GetSampleRate() const -> uint32_t = 0;
    [[nodiscard]] virtual auto GetChannelCount() const -> uint8_t = 0;
    [[nodiscard]] virtual auto GetFormat() const -> uint32_t = 0;
    [[nodiscard]] virtual auto GetDuration() const -> float = 0;
    [[nodiscard]] virtual auto GetSamples() const -> const int16_t* = 0;
    [[nodiscard]] virtual auto GetSampleCount() const -> uint64_t = 0;
};

} // namespace atom

#endif // ATOM_IAUDIO_BUFFER_HPP

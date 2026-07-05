#ifndef ATOM_IAUDIO_SOURCE_HPP
#define ATOM_IAUDIO_SOURCE_HPP

#include <cstdint>

namespace atom {

enum class AudioSourceState { Stopped, Playing, Paused };

class IAudioSource {
public:
    virtual ~IAudioSource() = default;

    // Playback control
    virtual auto Play() -> void = 0;
    virtual auto Stop() -> void = 0;
    virtual auto Pause() -> void = 0;
    [[nodiscard]] virtual auto GetState() const -> AudioSourceState = 0;

    // Volume (range 0.0 – 100.0)
    virtual auto SetVolume(float volume) -> void = 0;
    [[nodiscard]] virtual auto GetVolume() const -> float = 0;

    // Looping
    virtual auto SetLooping(bool loop) -> void = 0;
    [[nodiscard]] virtual auto IsLooping() const -> bool = 0;

    // Seek
    virtual auto SetPlayingOffset(float seconds) -> void = 0;
    [[nodiscard]] virtual auto GetPlayingOffset() const -> float = 0;

    // Bind raw PCM data (used by SFX sources — optional, default no-op)
    virtual auto SetBuffer(const uint8_t* data, uint32_t length) -> void {}
};

} // namespace atom

#endif // ATOM_IAUDIO_SOURCE_HPP

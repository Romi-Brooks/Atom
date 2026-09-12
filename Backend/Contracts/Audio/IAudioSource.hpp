#ifndef ATOM_IAUDIO_SOURCE_HPP
#define ATOM_IAUDIO_SOURCE_HPP

#include <cstdint>

namespace atom::audio {

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

        // Seek to a playback position, in seconds from the start.
        //
        // Returns false when the position cannot be reached: the id/position is
        // invalid, or the source is streaming from a forward-only decoder that
        // cannot seek there. Seeking a stopped or paused source sets the position
        // the next Play() starts from; seeking while playing continues playback
        // from the new position and may be audible as a short gap, because
        // already-queued audio has to be dropped.
        virtual auto SetPlayingOffset(float seconds) -> bool = 0;
        [[nodiscard]] virtual auto GetPlayingOffset() const -> float = 0;

        // True when SetPlayingOffset() can reach arbitrary positions.
        //
        // Music sources report the capability of the decoder they own. Short SFX
        // voices are not required to support seek at all; the default is the
        // conservative answer so a source never promises a seek it cannot do.
        [[nodiscard]] virtual auto IsSeekable() const -> bool {
            return false;
        }

        // Bind raw PCM data (used by SFX sources — optional, default no-op)
        virtual auto SetBuffer(const uint8_t* data, uint32_t length) -> void {}

        // Check if playback has finished naturally (stream dry).
        // Used by voice pools to determine voice reuse eligibility.
        // Defaults to false: a source that does not model completion must never
        // be reported as already finished, otherwise voice pools recycle it
        // before it produced a single sample.
        [[nodiscard]] virtual auto IsFinished() const -> bool {
            return false;
        }

        // Release every backend handle this source holds and become inert.
        //
        // BackendRuntime calls this (through IAudioBackend::Quiesce) immediately
        // before the backend that created the source is destroyed. Backends own
        // process-wide resources -- SDL_QuitSubSystem, for example, destroys
        // every audio stream in the process -- so a source that survives its
        // backend would otherwise keep dangling handles and crash on its next
        // call or in its destructor.
        //
        // After Detach(): every control call must be a safe no-op, and
        // destruction must not touch backend resources. Implementations must
        // make Detach() idempotent. The default no-op is only correct for
        // sources that own no backend resources.
        virtual auto Detach() -> void {}
};

} // namespace atom::audio

#endif // ATOM_IAUDIO_SOURCE_HPP

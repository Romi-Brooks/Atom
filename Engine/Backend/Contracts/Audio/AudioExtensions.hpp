#ifndef ATOM_AUDIO_EXTENSIONS_HPP
#define ATOM_AUDIO_EXTENSIONS_HPP

#include <algorithm>

#include <Backend/Contracts/Audio/IAudioSource.hpp>

namespace atom::audio {

struct AudioPosition {
        // Right-handed coordinates: X is left/right, Y is down/up, and Z is
        // forward/back. The mixer backend receives positions relative to its
        // listener at the origin.
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
};

class IPitchControl {
    public:
        virtual ~IPitchControl() = default;
        virtual auto SetPitch(float ratio) -> void = 0;
        [[nodiscard]] virtual auto GetPitch() const -> float = 0;
};

class IPanControl {
    public:
        virtual ~IPanControl() = default;
        virtual auto SetPan(float pan) -> void = 0;
        [[nodiscard]] virtual auto GetPan() const -> float = 0;
};

class ISpatialEmitter {
    public:
        virtual ~ISpatialEmitter() = default;
        virtual auto SetPosition(const AudioPosition& position) -> void = 0;
        virtual auto ClearPosition() -> void = 0;
        [[nodiscard]] virtual auto HasPosition() const -> bool = 0;
};

// Optional audio controls are queried at the call site. A backend that does
// not implement an extension simply leaves its normal playback untouched.
[[nodiscard]] inline auto TrySetPitch(IAudioSource& source, const float ratio) -> bool {
    if (auto* control = dynamic_cast<IPitchControl*>(&source)) {
        control->SetPitch(std::clamp(ratio, 0.01f, 100.0f));
        return true;
    }
    return false;
}

[[nodiscard]] inline auto TrySetPan(IAudioSource& source, const float pan) -> bool {
    if (auto* control = dynamic_cast<IPanControl*>(&source)) {
        control->SetPan(std::clamp(pan, -1.0f, 1.0f));
        return true;
    }
    return false;
}

[[nodiscard]] inline auto TrySetPosition(IAudioSource& source, const AudioPosition& position) -> bool {
    if (auto* emitter = dynamic_cast<ISpatialEmitter*>(&source)) {
        emitter->SetPosition(position);
        return true;
    }
    return false;
}

[[nodiscard]] inline auto TryClearPosition(IAudioSource& source) -> bool {
    if (auto* emitter = dynamic_cast<ISpatialEmitter*>(&source)) {
        emitter->ClearPosition();
        return true;
    }
    return false;
}

} // namespace atom::audio

#endif // ATOM_AUDIO_EXTENSIONS_HPP

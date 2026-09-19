#ifndef ATOM_DOPPLER_EFFECT_HPP
#define ATOM_DOPPLER_EFFECT_HPP

#include <Backend/Contracts/Audio/AudioExtensions.hpp>

#include "IAudioEffect.hpp"

namespace atom::audio {

// Position uses engine world units. Velocity uses world units per second.
struct SpatialMotion {
        AudioPosition position{};
        AudioPosition velocity{};
};

struct DopplerSettings {
        float speed_of_sound = 343.0f;
        float min_pitch_ratio = 0.5f;
        float max_pitch_ratio = 2.0f;
        float pan_distance = 10.0f;
        bool apply_3d_position = true;
        bool allow_stereo_pan_fallback = true;
};

enum class DopplerPanningMode { None, Position3D, StereoPanFallback };

struct DopplerResult {
        float pitch_ratio = 1.0f;
        float stereo_pan = 0.0f;
        float distance = 0.0f;
        float listener_radial_velocity = 0.0f;
        float emitter_radial_velocity = 0.0f;
        bool pitch_applied = false;
        DopplerPanningMode panning_mode = DopplerPanningMode::None;
};

// An effect-plugin object for Doppler pitch and spatial panning. It owns no
// audio device and does not require a particular backend implementation.
class DopplerEffect final : public IAudioEffect {
    public:
        auto SetListener(const SpatialMotion& listener) -> void;
        [[nodiscard]] auto GetListener() const -> const SpatialMotion&;
        auto SetEmitter(const SpatialMotion& emitter) -> void;
        [[nodiscard]] auto GetEmitter() const -> const SpatialMotion&;
        auto SetSettings(const DopplerSettings& settings) -> void;
        [[nodiscard]] auto GetSettings() const -> const DopplerSettings&;

        [[nodiscard]] auto Evaluate() const -> DopplerResult;
        auto Apply(IAudioSource& source) -> void override;
        [[nodiscard]] auto GetLastResult() const -> const DopplerResult&;

    private:
        SpatialMotion listener_{};
        SpatialMotion emitter_{};
        DopplerSettings settings_{};
        DopplerResult last_result_{};
};

} // namespace atom::audio

#endif // ATOM_DOPPLER_EFFECT_HPP

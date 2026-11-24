#include "DopplerEffect.hpp"

#include <algorithm>
#include <cmath>

#include <Backend/Contracts/Audio/IAudioSource.hpp>

namespace atom::audio {
namespace {

[[nodiscard]] auto Subtract(const AudioPosition& left, const AudioPosition& right) -> AudioPosition {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] auto Dot(const AudioPosition& left, const AudioPosition& right) -> float {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] auto Length(const AudioPosition& value) -> float {
    return std::sqrt(Dot(value, value));
}

[[nodiscard]] auto Scale(const AudioPosition& value, const float scale) -> AudioPosition {
    return {value.x * scale, value.y * scale, value.z * scale};
}

} // namespace

auto DopplerEffect::SetListener(const SpatialMotion& listener) -> void { listener_ = listener; }
auto DopplerEffect::GetListener() const -> const SpatialMotion& { return listener_; }
auto DopplerEffect::SetEmitter(const SpatialMotion& emitter) -> void { emitter_ = emitter; }
auto DopplerEffect::GetEmitter() const -> const SpatialMotion& { return emitter_; }

auto DopplerEffect::SetSettings(const DopplerSettings& settings) -> void {
    settings_ = settings;
    settings_.speed_of_sound = std::max(settings_.speed_of_sound, 1.0f);
    settings_.min_pitch_ratio = std::max(settings_.min_pitch_ratio, 0.01f);
    settings_.max_pitch_ratio = std::max(settings_.max_pitch_ratio, settings_.min_pitch_ratio);
    settings_.pan_distance = std::max(settings_.pan_distance, 0.01f);
}

auto DopplerEffect::GetSettings() const -> const DopplerSettings& { return settings_; }

auto DopplerEffect::Evaluate() const -> DopplerResult {
    DopplerResult result{};
    const AudioPosition relative_position = Subtract(emitter_.position, listener_.position);
    result.distance = Length(relative_position);
    result.stereo_pan = std::clamp(relative_position.x / settings_.pan_distance, -1.0f, 1.0f);
    if (result.distance <= 0.0001f)
        return result;

    const AudioPosition listener_to_emitter = Scale(relative_position, 1.0f / result.distance);
    result.listener_radial_velocity = Dot(listener_.velocity, listener_to_emitter);
    result.emitter_radial_velocity = Dot(emitter_.velocity, listener_to_emitter);

    // Positive listener radial velocity means the listener moves toward the
    // emitter. Positive emitter radial velocity means the emitter moves away.
    const float maximum_radial_speed = settings_.speed_of_sound * 0.95f;
    const float listener_velocity = std::clamp(result.listener_radial_velocity, -maximum_radial_speed,
                                               maximum_radial_speed);
    const float emitter_velocity = std::clamp(result.emitter_radial_velocity, -maximum_radial_speed,
                                              maximum_radial_speed);
    const float raw_ratio = (settings_.speed_of_sound + listener_velocity) /
                            (settings_.speed_of_sound + emitter_velocity);
    result.pitch_ratio = std::clamp(raw_ratio, settings_.min_pitch_ratio, settings_.max_pitch_ratio);
    return result;
}

auto DopplerEffect::Apply(IAudioSource& source) -> void {
    last_result_ = Evaluate();
    last_result_.pitch_applied = TrySetPitch(source, last_result_.pitch_ratio);

    const AudioPosition relative_position = Subtract(emitter_.position, listener_.position);
    if (settings_.apply_3d_position && TrySetPosition(source, relative_position)) {
        last_result_.panning_mode = DopplerPanningMode::Position3D;
        return;
    }
    if (settings_.allow_stereo_pan_fallback && TrySetPan(source, last_result_.stereo_pan))
        last_result_.panning_mode = DopplerPanningMode::StereoPanFallback;
}

auto DopplerEffect::GetLastResult() const -> const DopplerResult& { return last_result_; }

} // namespace atom::audio

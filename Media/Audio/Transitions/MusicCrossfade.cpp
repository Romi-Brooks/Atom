#include "MusicCrossfade.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
namespace atom::audio {
namespace {
constexpr float kHalfPi = 1.57079632679489661923f;
auto CurveName(const FadeCurve curve) -> const char* {
    switch (curve) {
    case FadeCurve::Linear:
        return "Linear";
    case FadeCurve::SmoothStep:
        return "SmoothStep";
    case FadeCurve::EqualPower:
        return "EqualPower";
    }
    return "Unknown";
}
auto FormatSeconds(const float seconds) -> std::string {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.2f", seconds);
    return buffer;
}
}
auto MusicCrossfade::Start(const std::string& target, const MusicCrossfadeConfig& config) -> bool {
    if (!player_.IsLoaded(target) || config.fade_out_duration < 0.0f || config.fade_in_duration < 0.0f) {
        LOG_ERROR(atom::audio::LogChannel::PLUG_MUSICFADE,
                  "MusicCrossfade: fade failed: target '" + target + "' not loaded or invalid duration");
        EnterState(MusicTransitionState::Failed);
        return false;
    }
    if (IsRunning()) {
        if (config.conflict_policy == TransitionConflictPolicy::Reject) {
            LOG_DEBUG(atom::audio::LogChannel::PLUG_MUSICFADE,
                      "MusicCrossfade: fade request rejected, transition already running (policy=Reject)");
            return false;
        }
        Cancel();
    }
    from_id_ = player_.GetNowPlaying();
    to_id_ = target;
    config_ = config;
    elapsed_ = 0.0f;
    peak_volume_ = player_.GetMusicVolume();
    if (from_id_ == to_id_) {
        LOG_INFO(atom::audio::LogChannel::PLUG_MUSICFADE,
                 "MusicCrossfade: target '" + to_id_ + "' is already playing, no fade needed");
        EnterState(MusicTransitionState::Completed);
        return true;
    }
    if (from_id_.empty()) {
        player_.Play(to_id_);
        LOG_INFO(atom::audio::LogChannel::PLUG_MUSICFADE,
                 "MusicCrossfade: started '" + to_id_ + "' directly (no current track to fade from)");
        EnterState(MusicTransitionState::Completed);
        return true;
    }
    LOG_INFO(atom::audio::LogChannel::PLUG_MUSICFADE,
             "MusicCrossfade: fade started '" + from_id_ + "' -> '" + to_id_ + "' (out=" +
                 FormatSeconds(config_.fade_out_duration) + "s, in=" + FormatSeconds(config_.fade_in_duration) +
                 "s, curve=" + CurveName(config_.curve) + ")");
    EnterState(MusicTransitionState::FadingOut);
    Update(0.0f);
    return true;
}
auto MusicCrossfade::Switch(const std::string& target, const float duration) -> bool {
    const auto phase = std::max(0.0f, duration * 0.5f);
    return Start(target, MusicCrossfadeConfig{phase, phase});
}
auto MusicCrossfade::Update(float delta_time) -> void {
    if (IsRunning() && !player_.IsLoaded(to_id_)) {
        LOG_ERROR(atom::audio::LogChannel::PLUG_MUSICFADE,
                  "MusicCrossfade: fade failed: target '" + to_id_ + "' was unloaded mid-fade");
        EnterState(MusicTransitionState::Failed);
        return;
    }
    auto remaining = std::max(0.0f, delta_time);
    for (int phase_count = 0; phase_count < 2 && IsRunning(); ++phase_count) {
        const bool fading_out = state_ == MusicTransitionState::FadingOut;
        const float duration = fading_out ? config_.fade_out_duration : config_.fade_in_duration;
        const float needed = std::max(0.0f, duration - elapsed_);
        const float consumed = duration <= 0.0f ? 0.0f : std::min(remaining, needed);
        elapsed_ += consumed;
        remaining -= consumed;
        const float progress = duration <= 0.0f ? 1.0f : std::clamp(elapsed_ / duration, 0.0f, 1.0f);
        player_.SetVolume(fading_out ? from_id_ : to_id_,
                          peak_volume_ * EvaluateCurve(progress, config_.curve, !fading_out));
        if (progress < 1.0f)
            break;
        elapsed_ = 0.0f;
        if (fading_out) {
            player_.Stop(from_id_);
            player_.Play(to_id_, 0.0f);
            LOG_INFO(atom::audio::LogChannel::PLUG_MUSICFADE,
                     "MusicCrossfade: faded out '" + from_id_ + "', fading in '" + to_id_ + "'");
            EnterState(MusicTransitionState::FadingIn);
        } else {
            player_.SetVolume(to_id_, peak_volume_);
            LOG_INFO(atom::audio::LogChannel::PLUG_MUSICFADE,
                     "MusicCrossfade: fade completed '" + from_id_ + "' -> '" + to_id_ + "'");
            Complete();
        }
    }
}
auto MusicCrossfade::Cancel() -> void {
    if (!IsRunning())
        return;
    const auto& active = state_ == MusicTransitionState::FadingOut ? from_id_ : to_id_;
    if (!active.empty())
        player_.SetVolume(active, peak_volume_);
    LOG_DEBUG(atom::audio::LogChannel::PLUG_MUSICFADE,
              "MusicCrossfade: fade cancelled '" + from_id_ + "' -> '" + to_id_ + "'");
    EnterState(MusicTransitionState::Cancelled);
}
auto MusicCrossfade::Reset() -> void {
    if (IsRunning())
        Cancel();
    state_ = MusicTransitionState::Idle;
    from_id_.clear();
    to_id_.clear();
    elapsed_ = 0.0f;
}
auto MusicCrossfade::SetCallback(Callback callback) -> void {
    callback_ = std::move(callback);
}
auto MusicCrossfade::EvaluateCurve(float progress, const FadeCurve curve, const bool fade_in) -> float {
    progress = std::clamp(progress, 0.0f, 1.0f);
    switch (curve) {
    case FadeCurve::Linear:
        return fade_in ? progress : 1.0f - progress;
    case FadeCurve::SmoothStep: {
        const auto smooth = progress * progress * (3.0f - 2.0f * progress);
        return fade_in ? smooth : 1.0f - smooth;
    }
    case FadeCurve::EqualPower:
        return fade_in ? std::sin(progress * kHalfPi) : std::cos(progress * kHalfPi);
    }
    return fade_in ? progress : 1.0f - progress;
}
auto MusicCrossfade::EnterState(const MusicTransitionState state) -> void {
    state_ = state;
    auto callback = callback_;
    const auto from = from_id_;
    const auto to = to_id_;
    if (callback)
        callback(state, from, to);
}
auto MusicCrossfade::Complete() -> void {
    EnterState(MusicTransitionState::Completed);
}
auto MusicCrossfade::GetState() const -> MusicTransitionState {
    return state_;
}
auto MusicCrossfade::GetProgress() const -> float {
    if (state_ == MusicTransitionState::Completed)
        return 1.0f;
    if (!IsRunning())
        return 0.0f;
    const auto duration =
        state_ == MusicTransitionState::FadingOut ? config_.fade_out_duration : config_.fade_in_duration;
    const auto phase = duration <= 0.0f ? 1.0f : std::clamp(elapsed_ / duration, 0.0f, 1.0f);
    return state_ == MusicTransitionState::FadingOut ? phase * 0.5f : 0.5f + phase * 0.5f;
}
auto MusicCrossfade::GetFromId() const -> const std::string& {
    return from_id_;
}
auto MusicCrossfade::GetToId() const -> const std::string& {
    return to_id_;
}
auto MusicCrossfade::GetDuration() const -> float {
    return config_.fade_out_duration + config_.fade_in_duration;
}
auto MusicCrossfade::IsRunning() const -> bool {
    return state_ == MusicTransitionState::FadingOut || state_ == MusicTransitionState::FadingIn;
}
} // namespace atom::audio

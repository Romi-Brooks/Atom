#include "SDL3MixerSource.hpp"

#include <algorithm>

#include <SDL3_mixer/SDL_mixer.h>

#include "SDL3MixerContext.hpp"
#include <Log/LogSystem.hpp>

namespace atom::backend::sdl3mixer {

SDL3MixerSource::SDL3MixerSource(std::shared_ptr<SDL3MixerContext> context, const uint8_t* pcm,
                                 const std::size_t length, const SDL_AudioSpec& spec)
    : context_(std::move(context)), spec_(spec) {
    if (!context_ || !context_->IsReady() || !pcm || length == 0)
        return;
    audio_ = MIX_LoadRawAudio(context_->Mixer(), pcm, length, &spec_);
    if (!audio_) {
        LOG_ERROR(atom::log::backend::Audio::sdl3_mixer, "PCM load failed: " + std::string(SDL_GetError()));
        return;
    }
    track_ = MIX_CreateTrack(context_->Mixer());
    if (!track_ || !MIX_SetTrackAudio(track_, audio_)) {
        LOG_ERROR(atom::log::backend::Audio::sdl3_mixer, "track creation failed: " + std::string(SDL_GetError()));
        if (track_)
            MIX_DestroyTrack(track_);
        track_ = nullptr;
        MIX_DestroyAudio(audio_);
        audio_ = nullptr;
    }
}

SDL3MixerSource::~SDL3MixerSource() {
    Detach();
}

auto SDL3MixerSource::ReleaseBackendHandles() -> void {
    Stop();
    if (track_) {
        MIX_DestroyTrack(track_);
        track_ = nullptr;
    }
    if (audio_) {
        MIX_DestroyAudio(audio_);
        audio_ = nullptr;
    }
    positioned_ = false;
    pan_requested_ = false;
    // Dropping the context closes the door on any later MIX call from this
    // source, and releases the shared mixer once the last holder is gone.
    context_.reset();
}

auto SDL3MixerSource::PlayOptions() const -> SDL_PropertiesID {
    const SDL_PropertiesID options = SDL_CreateProperties();
    if (options != 0)
        SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, looping_.load() ? -1 : 0);
    return options;
}

auto SDL3MixerSource::Play() -> void {
    if (!track_)
        return;
    if (MIX_TrackPaused(track_)) {
        MIX_ResumeTrack(track_);
        return;
    }
    const SDL_PropertiesID options = PlayOptions();
    const bool played = MIX_PlayTrack(track_, options);
    if (options != 0)
        SDL_DestroyProperties(options);
    if (!played)
        LOG_ERROR(atom::log::backend::Audio::sdl3_mixer, "track play failed: " + std::string(SDL_GetError()));
    else
        LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "static track started, loop=" + std::to_string(looping_.load()));
}

auto SDL3MixerSource::Stop() -> void {
    if (track_ && MIX_TrackPlaying(track_))
        MIX_StopTrack(track_, 0);
    LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "static track stopped");
}

auto SDL3MixerSource::Pause() -> void {
    if (track_ && MIX_TrackPlaying(track_))
        MIX_PauseTrack(track_);
    LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "static track paused");
}

auto SDL3MixerSource::GetState() const -> atom::audio::AudioSourceState {
    if (!track_ || !MIX_TrackPlaying(track_))
        return atom::audio::AudioSourceState::Stopped;
    return MIX_TrackPaused(track_) ? atom::audio::AudioSourceState::Paused : atom::audio::AudioSourceState::Playing;
}

auto SDL3MixerSource::SetVolume(const float volume) -> void {
    const float clamped = std::clamp(volume, 0.0f, 100.0f);
    if (volume_.exchange(clamped) == clamped)
        return;
    if (track_)
        MIX_SetTrackGain(track_, volume_.load() / 100.0f);
    LOG_DEBUG(atom::log::backend::Audio::sdl3_mixer, "track volume=" + std::to_string(volume_.load()));
}

auto SDL3MixerSource::GetVolume() const -> float { return volume_.load(); }

auto SDL3MixerSource::SetLooping(const bool loop) -> void {
    if (looping_.exchange(loop) == loop)
        return;
    if (track_ && MIX_TrackPlaying(track_))
        MIX_SetTrackLoops(track_, loop ? -1 : 0);
    LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "track loop=" + std::to_string(loop));
}

auto SDL3MixerSource::IsLooping() const -> bool { return looping_.load(); }

auto SDL3MixerSource::SetPlayingOffset(const float seconds) -> bool {
    // Fully buffered audio: MIX owns the decoded frames, so an arbitrary seek is
    // possible (unlike the streaming sources, whose decoder has to support it).
    if (!track_ || spec_.freq <= 0)
        return false;
    return MIX_SetTrackPlaybackPosition(track_, static_cast<Sint64>(std::max(seconds, 0.0f) * spec_.freq));
}

auto SDL3MixerSource::GetPlayingOffset() const -> float {
    if (!track_ || spec_.freq <= 0)
        return 0.0f;
    return static_cast<float>(std::max<Sint64>(0, MIX_GetTrackPlaybackPosition(track_))) / spec_.freq;
}

auto SDL3MixerSource::IsSeekable() const -> bool {
    return track_ != nullptr && spec_.freq > 0;
}

auto SDL3MixerSource::IsFinished() const -> bool { return GetState() == atom::audio::AudioSourceState::Stopped; }

auto SDL3MixerSource::SetPitch(const float ratio) -> void {
    const float clamped = std::clamp(ratio, 0.01f, 100.0f);
    if (pitch_.exchange(clamped) == clamped)
        return;
    if (track_)
        MIX_SetTrackFrequencyRatio(track_, pitch_.load());
    LOG_DEBUG(atom::log::backend::Audio::sdl3_mixer, "track pitch=" + std::to_string(pitch_.load()));
}

auto SDL3MixerSource::GetPitch() const -> float { return pitch_.load(); }

auto SDL3MixerSource::ApplyPan() -> void {
    if (!track_)
        return;
    const float pan = pan_.load();
    const MIX_StereoGains gains{std::clamp(1.0f - pan, 0.0f, 1.0f), std::clamp(1.0f + pan, 0.0f, 1.0f)};
    MIX_SetTrackStereo(track_, &gains);
}

auto SDL3MixerSource::SetPan(const float pan) -> void {
    const float clamped = std::clamp(pan, -1.0f, 1.0f);
    const bool changed = pan_.exchange(clamped) != clamped;
    // MIX_SetTrackStereo is a spatialization *mode* switch, not just a gain
    // update: it also clears the 3D position. Deduplicating on the pan value
    // alone would leave a previously positioned track in 3D mode, so re-apply
    // whenever the track is still positioned.
    const bool leaving_3d = positioned_.exchange(false);
    pan_requested_ = true;
    if (track_ && (changed || leaving_3d)) {
        ApplyPan();
        LOG_DEBUG(atom::log::backend::Audio::sdl3_mixer, "track pan=" + std::to_string(pan_.load()));
    }
}

auto SDL3MixerSource::GetPan() const -> float { return pan_.load(); }

auto SDL3MixerSource::SetPosition(const atom::audio::AudioPosition& position) -> void {
    if (!track_)
        return;
    const MIX_Point3D point{position.x, position.y, position.z};
    if (MIX_SetTrack3DPosition(track_, &point))
        positioned_ = true;
}

auto SDL3MixerSource::ClearPosition() -> void {
    if (track_)
        MIX_SetTrack3DPosition(track_, nullptr);
    positioned_ = false;
    // Dropping back to MIX_SPATIALIZATION_NONE also drops the stereo gains, so
    // restore an explicitly requested pan instead of leaving the track centred.
    if (track_ && pan_requested_.load())
        ApplyPan();
}

auto SDL3MixerSource::HasPosition() const -> bool { return positioned_.load(); }
auto SDL3MixerSource::IsValid() const -> bool { return track_ != nullptr && audio_ != nullptr; }

} // namespace atom::backend::sdl3mixer

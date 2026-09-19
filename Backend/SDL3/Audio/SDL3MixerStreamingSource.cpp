#include "SDL3MixerStreamingSource.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "SDL3MixerContext.hpp"
#include <Log/LogSystem.hpp>

namespace atom::backend::sdl3mixer {

namespace {
// Keep the mixer's input stream fed, but not unboundedly: once this much audio
// is queued the decode thread sleeps instead of spinning.
constexpr int kSDLQueueTarget = 128 * 1024;
constexpr std::size_t kDecodeChunkBytes = 64 * 1024;
constexpr auto kIdleSleep = std::chrono::milliseconds{5};
} // namespace

SDL3MixerStreamingSource::SDL3MixerStreamingSource(std::shared_ptr<SDL3MixerContext> context,
                                                   std::unique_ptr<atom::audio::IAudioDecoder> decoder,
                                                   const SDL_AudioSpec& spec)
    : context_(std::move(context)), decoder_(std::move(decoder)), spec_(spec) {
    if (!context_ || !context_->IsReady() || !decoder_ || !decoder_->IsOpen())
        return;

    // Decoder format on the way in, mixer track format on the way out; SDL
    // converts between them.
    stream_ = SDL_CreateAudioStream(&spec_, &spec_);
    track_ = stream_ ? MIX_CreateTrack(context_->Mixer()) : nullptr;
    if (!track_ || !MIX_SetTrackAudioStream(track_, stream_)) {
        LOG_ERROR(atom::log::backend::Audio::sdl3_mixer,
                  "streaming track creation failed: " + std::string(SDL_GetError()));
        if (track_) {
            MIX_DestroyTrack(track_);
            track_ = nullptr;
        }
        if (stream_) {
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
        }
        return;
    }

    MIX_SetTrackGain(track_, volume_.load() / 100.0f);
    MIX_SetTrackFrequencyRatio(track_, pitch_.load());
    LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "streaming decoder source initialized");
}

SDL3MixerStreamingSource::~SDL3MixerStreamingSource() {
    Detach();
}

auto SDL3MixerStreamingSource::ReleaseBackendHandles() -> void {
    Stop();
    if (track_) {
        MIX_DestroyTrack(track_);
        track_ = nullptr;
    }
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
    // Releasing the decoder makes every later Play() a no-op.
    if (decoder_)
        decoder_->Close();
    decoder_.reset();
    finished_ = false;
    positioned_ = false;
    pan_requested_ = false;
    context_.reset();
}

auto SDL3MixerStreamingSource::Play() -> void {
    if (!track_ || !stream_ || !decoder_ || !decoder_->IsOpen())
        return;
    if (state_.load() == atom::audio::AudioSourceState::Playing)
        return;

    // Stop (and reap) any worker left over from a previous run *before* the
    // state says "playing", otherwise a stale worker would never exit.
    const bool resuming = state_.load() == atom::audio::AudioSourceState::Paused;
    running_ = false;
    if (worker_.joinable())
        worker_.join();

    if (resuming) {
        state_ = atom::audio::AudioSourceState::Playing;
        MIX_ResumeTrack(track_);
        // The decode thread left when the source was paused; start a fresh one so
        // the stream is fed again.
        running_ = true;
        worker_ = std::thread{&SDL3MixerStreamingSource::DecodeLoop, this};
        return;
    }

    // A fresh start either honours the position set by SetPlayingOffset() or
    // rewinds to the beginning.
    if (decoder_positioned_.exchange(false)) {
        LOG_DEBUG(atom::log::backend::Audio::sdl3_mixer,
                  "Play(): starting from the position set by SetPlayingOffset()");
    } else {
        if (!decoder_->Rewind())
            LOG_WARNING(atom::log::backend::Audio::sdl3_mixer, "Play(): decoder rewind failed");
        position_base_frames_ = 0;
    }

    BeginDecoding();
}

auto SDL3MixerStreamingSource::HaltDecoding() -> void {
    running_ = false;
    if (worker_.joinable())
        worker_.join();
    if (track_)
        MIX_StopTrack(track_, 0);
    if (stream_)
        SDL_ClearAudioStream(stream_);
    state_ = atom::audio::AudioSourceState::Stopped;
    finished_ = false;
    frames_pushed_ = 0;
    decoder_positioned_ = false;
}

auto SDL3MixerStreamingSource::BeginDecoding() -> void {
    if (worker_.joinable())
        worker_.join();
    SDL_ClearAudioStream(stream_);
    finished_ = false;
    frames_pushed_ = 0;

    // A just-in-time stream is legitimately empty for a moment (the first poll
    // happens before the decode thread pushed anything, and a slow decode can
    // drain the queue later). MIX treats "no data right now" as end of audio and
    // halts the track unless halt_when_exhausted is disabled, so looping is
    // handled by our own decoder rewind instead of MIX_PROP_PLAY_LOOPS_NUMBER
    // (MIX cannot loop an audio-stream input at all).
    const SDL_PropertiesID options = SDL_CreateProperties();
    if (options != 0) {
        SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, 0);
        SDL_SetBooleanProperty(options, MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN, false);
    }

    state_ = atom::audio::AudioSourceState::Playing;
    running_ = true;
    worker_ = std::thread{&SDL3MixerStreamingSource::DecodeLoop, this};
    const bool started = MIX_PlayTrack(track_, options);
    if (options != 0)
        SDL_DestroyProperties(options);
    if (!started)
        LOG_ERROR(atom::log::backend::Audio::sdl3_mixer, "track play failed: " + std::string(SDL_GetError()));
}

auto SDL3MixerStreamingSource::Stop() -> void {
    HaltDecoding();
    position_base_frames_ = 0;
}

auto SDL3MixerStreamingSource::Pause() -> void {
    if (state_.load() != atom::audio::AudioSourceState::Playing)
        return;
    running_ = false;
    if (worker_.joinable())
        worker_.join();
    if (track_)
        MIX_PauseTrack(track_);
    state_ = atom::audio::AudioSourceState::Paused;
}

auto SDL3MixerStreamingSource::GetState() const -> atom::audio::AudioSourceState {
    return state_.load();
}

auto SDL3MixerStreamingSource::SetVolume(const float volume) -> void {
    volume_ = volume;
    if (track_)
        MIX_SetTrackGain(track_, volume / 100.0f);
    LOG_DEBUG(atom::log::backend::Audio::sdl3_mixer, "streaming volume=" + std::to_string(volume));
}

auto SDL3MixerStreamingSource::GetVolume() const -> float {
    return volume_.load();
}

auto SDL3MixerStreamingSource::SetLooping(const bool loop) -> void {
    loop_ = loop;
    // Deliberately no MIX_SetTrackLoops() here: MIX cannot loop an
    // audio-stream input (it halts the track instead), so looping is performed
    // by the decode thread rewinding the decoder.
    LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "streaming loop=" + std::to_string(loop));
}

auto SDL3MixerStreamingSource::IsLooping() const -> bool {
    return loop_.load();
}

auto SDL3MixerStreamingSource::SetPlayingOffset(const float seconds) -> bool {
    if (!track_ || !decoder_ || !decoder_->IsOpen() || spec_.freq <= 0)
        return false;

    const auto target_frame = static_cast<std::uint64_t>(std::max(seconds, 0.0f) * static_cast<float>(spec_.freq));

    // MIX cannot seek a track that plays an SDL_AudioStream
    // (SDL_mixer.c: "can't seek a stream that was set up with
    // MIX_SetTrackAudioStream"), so the seek happens on our decoder and the
    // track is restarted from the new data. Decoders that can only decode
    // forward still support "back to the start".
    if (target_frame != 0 && !decoder_->IsSeekable()) {
        LOG_WARNING(atom::log::backend::Audio::sdl3_mixer,
                    "SetPlayingOffset(" + std::to_string(seconds) + "s) ignored: the decoder only decodes forward");
        return false;
    }

    const bool was_playing = state_.load() == atom::audio::AudioSourceState::Playing;
    HaltDecoding();

    if (!decoder_->SeekToFrame(target_frame)) {
        LOG_WARNING(atom::log::backend::Audio::sdl3_mixer, "SetPlayingOffset(" + std::to_string(seconds) +
                                                              "s) failed: the decoder rejected the position");
        return false;
    }
    position_base_frames_ = target_frame;

    if (was_playing) {
        BeginDecoding();
    } else {
        // A later Play() must start here instead of rewinding over the seek.
        decoder_positioned_ = true;
    }
    LOG_INFO(atom::log::backend::Audio::sdl3_mixer,
             "streaming playback seek to " + std::to_string(static_cast<double>(seconds)) + "s (frame " +
                 std::to_string(target_frame) + ")");
    return true;
}

auto SDL3MixerStreamingSource::GetPlayingOffset() const -> float {
    if (!track_ || spec_.freq <= 0)
        return 0.0f;
    const auto bytes_per_frame = static_cast<std::uint64_t>(SDL_AUDIO_BYTESIZE(spec_.format)) * spec_.channels;
    if (bytes_per_frame == 0)
        return 0.0f;

    // Progress = frames pushed minus frames the mixer has not consumed yet.
    // MIX's own playback position cannot be used: it keeps counting across loop
    // wraps, so it would run past the end of the track.
    auto played = frames_pushed_.load();
    if (stream_) {
        const int queued = SDL_GetAudioStreamQueued(stream_);
        const auto queued_frames = static_cast<std::uint64_t>(queued > 0 ? queued : 0) / bytes_per_frame;
        played = queued_frames < played ? played - queued_frames : 0;
    }
    const auto absolute_frames = position_base_frames_.load() + played;
    return static_cast<float>(absolute_frames) / static_cast<float>(spec_.freq);
}

auto SDL3MixerStreamingSource::IsSeekable() const -> bool {
    return track_ != nullptr && decoder_ && decoder_->IsOpen() && decoder_->IsSeekable();
}

auto SDL3MixerStreamingSource::IsFinished() const -> bool {
    return finished_.load();
}

auto SDL3MixerStreamingSource::SetPitch(const float ratio) -> void {
    const float clamped = std::clamp(ratio, 0.01f, 100.0f);
    if (pitch_.exchange(clamped) == clamped)
        return;
    if (track_)
        MIX_SetTrackFrequencyRatio(track_, clamped);
    LOG_DEBUG(atom::log::backend::Audio::sdl3_mixer, "streaming pitch=" + std::to_string(clamped));
}

auto SDL3MixerStreamingSource::GetPitch() const -> float {
    return pitch_.load();
}

auto SDL3MixerStreamingSource::ApplyPan() -> void {
    if (!track_)
        return;
    const float pan = pan_.load();
    const MIX_StereoGains gains{std::clamp(1.0f - pan, 0.0f, 1.0f), std::clamp(1.0f + pan, 0.0f, 1.0f)};
    MIX_SetTrackStereo(track_, &gains);
}

auto SDL3MixerStreamingSource::SetPan(const float pan) -> void {
    const float clamped = std::clamp(pan, -1.0f, 1.0f);
    const bool changed = pan_.exchange(clamped) != clamped;
    // See SDL3MixerSource::SetPan: stereo pan and 3D position are mutually
    // exclusive spatialization modes, so the mode switch must not be skipped just
    // because the pan value itself did not change.
    const bool leaving_3d = positioned_.exchange(false);
    pan_requested_ = true;
    if (track_ && (changed || leaving_3d)) {
        ApplyPan();
        LOG_DEBUG(atom::log::backend::Audio::sdl3_mixer, "streaming pan=" + std::to_string(pan_.load()));
    }
}

auto SDL3MixerStreamingSource::GetPan() const -> float {
    return pan_.load();
}

auto SDL3MixerStreamingSource::SetPosition(const atom::audio::AudioPosition& position) -> void {
    if (!track_)
        return;
    const MIX_Point3D point{position.x, position.y, position.z};
    if (MIX_SetTrack3DPosition(track_, &point))
        positioned_ = true;
}

auto SDL3MixerStreamingSource::ClearPosition() -> void {
    if (track_)
        MIX_SetTrack3DPosition(track_, nullptr);
    positioned_ = false;
    if (track_ && pan_requested_.load())
        ApplyPan();
}

auto SDL3MixerStreamingSource::HasPosition() const -> bool {
    return positioned_.load();
}

auto SDL3MixerStreamingSource::IsValid() const -> bool {
    return track_ != nullptr && stream_ != nullptr;
}

auto SDL3MixerStreamingSource::DecodeLoop() -> void {
    std::array<uint8_t, kDecodeChunkBytes> buffer{};
    const auto bytes_per_frame =
        static_cast<std::uint64_t>(SDL_AUDIO_BYTESIZE(spec_.format)) * static_cast<std::uint64_t>(spec_.channels);

    while (running_.load() && state_.load() == atom::audio::AudioSourceState::Playing) {
        if (SDL_GetAudioStreamQueued(stream_) >= kSDLQueueTarget) {
            std::this_thread::sleep_for(kIdleSleep);
            continue;
        }

        const auto decoded =
            decoder_ ? decoder_->DecodeChunk(buffer.data(), static_cast<uint32_t>(buffer.size())) : 0U;
        if (decoded > 0) {
            if (!SDL_PutAudioStreamData(stream_, buffer.data(), static_cast<int>(decoded))) {
                LOG_ERROR(atom::log::backend::Audio::sdl3_mixer,
                          "SDL_PutAudioStreamData failed: " + std::string(SDL_GetError()));
                break;
            }
            if (bytes_per_frame > 0)
                frames_pushed_.fetch_add(decoded / bytes_per_frame);
            continue;
        }

        // 0 bytes means EOF (or a decoder error).
        if (loop_.load() && decoder_ && decoder_->Rewind()) {
            SDL_FlushAudioStream(stream_);
            // The next pass starts at the beginning of the track again, so the
            // position accounting must wrap with it.
            frames_pushed_ = 0;
            position_base_frames_ = 0;
            continue;
        }

        // Flush the converter so the final resampler tail is not stranded, then
        // wait for the device to drain before reporting completion.
        SDL_FlushAudioStream(stream_);
        while (running_.load() && SDL_GetAudioStreamAvailable(stream_) > 0)
            std::this_thread::sleep_for(kIdleSleep);

        if (running_.load() && state_.load() == atom::audio::AudioSourceState::Playing) {
            if (track_)
                MIX_StopTrack(track_, 0);
            state_ = atom::audio::AudioSourceState::Stopped;
            finished_ = true;
            LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "streaming playback completed (end of data)");
        }
        break;
    }

    running_ = false;
}

} // namespace atom::backend::sdl3mixer

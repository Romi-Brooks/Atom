#include "SDL3SFXSource.hpp"

#include <algorithm>

#include <SDL3/SDL.h>

#include <Log/LogSystem.hpp>

namespace atom::backend::sdl3 {

SDL3SFXSource::SDL3SFXSource() = default;

SDL3SFXSource::~SDL3SFXSource() {
    // Detach() rather than ReleaseBackendHandles(): a source destroyed outside a
    // backend switch must also leave the backend's source registry.
    Detach();
}

auto SDL3SFXSource::SetSpec(const SDL_AudioSpec& spec) -> void {
    spec_ = spec;
    LOG_DEBUG(atom::log::backend::Audio::sdl3, "SetSpec: fmt=" + std::to_string(spec_.format) +
                                                          " freq=" + std::to_string(spec_.freq) +
                                                          " ch=" + std::to_string(spec_.channels));
}

auto SDL3SFXSource::EnsureStream() -> bool {
    if (stream_)
        return true;
    if (spec_.format == 0) {
        LOG_ERROR(atom::log::backend::Audio::sdl3, "EnsureStream aborted: format=0 (spec not set)");
        return false;
    }

    LOG_DEBUG(atom::log::backend::Audio::sdl3, "Opening stream: fmt=" + std::to_string(spec_.format) +
                                                          " freq=" + std::to_string(spec_.freq) +
                                                          " ch=" + std::to_string(spec_.channels));

    // The callback is the single writer of feed_cursor_, so looping and
    // completion stay consistent no matter how often Play() is called.
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, &SDL3SFXSource::FeedCallback,
                                       this);
    if (!stream_) {
        LOG_ERROR(atom::log::backend::Audio::sdl3,
                  "SDL_OpenAudioDeviceStream failed: " + std::string(SDL_GetError()));
        return false;
    }
    SDL_SetAudioStreamGain(stream_, volume_.load() / 100.0f);

    LOG_DEBUG(atom::log::backend::Audio::sdl3, "Audio stream opened");
    return true;
}

auto SDL3SFXSource::SetBuffer(const uint8_t* data, uint32_t length) -> void {
    pcm_data_.assign(data, data + length);
    feed_cursor_ = 0;
    drained_ = false;
    LOG_DEBUG(atom::log::backend::Audio::sdl3, "SetBuffer: " + std::to_string(length) + " bytes");
}

auto SDL3SFXSource::Feed(SDL_AudioStream& stream, const int additional_amount) -> void {
    if (state_.load() != atom::audio::AudioSourceState::Playing)
        return;
    if (additional_amount <= 0 || pcm_data_.empty())
        return;

    const auto cursor = feed_cursor_.load();
    if (cursor >= pcm_data_.size()) {
        if (!looping_.load()) {
            drained_ = true;
            return;
        }
        feed_cursor_ = 0;
    }

    const auto effective_cursor = feed_cursor_.load();
    const auto remaining = pcm_data_.size() - effective_cursor;
    const auto to_push = (std::min)(remaining, static_cast<std::size_t>(additional_amount));
    if (to_push == 0)
        return;

    if (!SDL_PutAudioStreamData(&stream, pcm_data_.data() + effective_cursor, static_cast<int>(to_push))) {
        // No logging from the audio thread; report completion so the voice is
        // reclaimed instead of spinning on a broken stream.
        drained_ = true;
        return;
    }
    feed_cursor_.store(effective_cursor + to_push);
}

void SDLCALL SDL3SFXSource::FeedCallback(void* userdata, SDL_AudioStream* stream, const int additional_amount,
                                         int /*total_amount*/) {
    if (userdata == nullptr || stream == nullptr)
        return;
    static_cast<SDL3SFXSource*>(userdata)->Feed(*stream, additional_amount);
}

auto SDL3SFXSource::Play() -> void {
    if (pcm_data_.empty()) {
        LOG_WARNING(atom::log::backend::Audio::sdl3, "Play() called with empty data");
        return;
    }
    if (!EnsureStream()) {
        LOG_WARNING(atom::log::backend::Audio::sdl3, "Play() aborted: cannot open stream");
        return;
    }

    // Resume a paused voice from where it stopped; every other entry restarts
    // from the beginning unless SetPlayingOffset() asked for a position.
    if (state_.load() != atom::audio::AudioSourceState::Paused) {
        if (!seek_pending_.exchange(false)) {
            feed_cursor_ = 0;
        }
        SDL_ClearAudioStream(stream_);
    }
    seek_pending_ = false;
    drained_ = false;
    SDL_SetAudioStreamGain(stream_, volume_.load() / 100.0f);

    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        LOG_ERROR(atom::log::backend::Audio::sdl3,
                  "SDL_ResumeAudioStreamDevice failed: " + std::string(SDL_GetError()));
        return;
    }

    state_ = atom::audio::AudioSourceState::Playing;
    LOG_DEBUG(atom::log::backend::Audio::sdl3,
              "Playback started: " + std::to_string(pcm_data_.size()) + " bytes, loop=" +
                  std::to_string(looping_.load() ? 1 : 0));
}

auto SDL3SFXSource::Stop() -> void {
    state_ = atom::audio::AudioSourceState::Stopped;
    feed_cursor_ = 0;
    drained_ = false;
    if (!stream_)
        return;
    SDL_ClearAudioStream(stream_);
    SDL_PauseAudioStreamDevice(stream_);
    LOG_DEBUG(atom::log::backend::Audio::sdl3, "Playback stopped");
}

auto SDL3SFXSource::Pause() -> void {
    if (!stream_)
        return;
    state_ = atom::audio::AudioSourceState::Paused;
    SDL_PauseAudioStreamDevice(stream_);
    LOG_DEBUG(atom::log::backend::Audio::sdl3, "Playback paused");
}

auto SDL3SFXSource::GetState() const -> atom::audio::AudioSourceState {
    return state_.load();
}

auto SDL3SFXSource::SetVolume(const float volume) -> void {
    volume_ = volume;
    if (stream_) {
        SDL_SetAudioStreamGain(stream_, volume_ / 100.0f);
    }
}

auto SDL3SFXSource::GetVolume() const -> float {
    return volume_.load();
}

auto SDL3SFXSource::SetLooping(bool loop) -> void {
    looping_ = loop;
    if (!loop)
        drained_ = false;
}

auto SDL3SFXSource::IsLooping() const -> bool {
    return looping_.load();
}

auto SDL3SFXSource::SetPlayingOffset(const float seconds) -> bool {
    const auto bytes_per_frame = static_cast<std::size_t>(SDL_AUDIO_BYTESIZE(spec_.format)) * spec_.channels;
    if (bytes_per_frame == 0 || spec_.freq == 0 || pcm_data_.empty())
        return false;
    const auto target = static_cast<std::size_t>(std::max(seconds, 0.0f) * static_cast<float>(spec_.freq)) *
                        bytes_per_frame;
    feed_cursor_ = (std::min)(target, pcm_data_.size());
    drained_ = false;
    seek_pending_ = true;
    if (stream_)
        SDL_ClearAudioStream(stream_);
    return true;
}

auto SDL3SFXSource::GetPlayingOffset() const -> float {
    const auto bytes_per_frame = static_cast<std::size_t>(SDL_AUDIO_BYTESIZE(spec_.format)) * spec_.channels;
    if (bytes_per_frame == 0 || spec_.freq == 0)
        return 0.0f;
    return static_cast<float>(feed_cursor_.load()) /
           (static_cast<float>(spec_.freq) * static_cast<float>(bytes_per_frame));
}

auto SDL3SFXSource::IsSeekable() const -> bool {
    // Not a design requirement for short voices, but the feed cursor makes it
    // exact, so the capability is advertised rather than hidden.
    return !pcm_data_.empty() && spec_.format != 0 && spec_.freq != 0;
}

auto SDL3SFXSource::IsFinished() const -> bool {
    // "Finished" means: every byte was handed to the device, no loop is
    // requested, and the device queue has drained. A paused or stopped voice is
    // not reported as finished -- voice pools ask only about playing voices.
    if (looping_.load() || !drained_.load())
        return false;
    if (!stream_)
        return true;
    return SDL_GetAudioStreamAvailable(stream_) == 0;
}

auto SDL3SFXSource::ReleaseBackendHandles() -> void {
    state_ = atom::audio::AudioSourceState::Stopped;
    feed_cursor_ = 0;
    drained_ = false;
    seek_pending_ = false;
    // Dropping the buffer makes every later Play() a no-op, so a detached voice
    // can never reopen a stream on a backend that no longer owns it.
    pcm_data_.clear();
    if (stream_) {
        // SDL owns the logical device of a simplified stream, so destroying the
        // stream closes it. Doing this from the caller's thread (not from the
        // audio callback) is what SDL_DestroyAudioStream expects.
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
}

} // namespace atom::backend::sdl3

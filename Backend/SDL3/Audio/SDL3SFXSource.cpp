#include "SDL3SFXSource.hpp"

#include <SDL3/SDL.h>

#include <Log/LogSystem.hpp>

namespace atom {

SDL3SFXSource::SDL3SFXSource() = default;

SDL3SFXSource::~SDL3SFXSource() {
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
    }
}

auto SDL3SFXSource::SetSpec(const SDL_AudioSpec& spec) -> void {
    spec_ = spec;
    LOG_DEBUG(atom::backend::sdl::LogChannel::AUDIO, "SetSpec: fmt=" + std::to_string(spec_.format) +
                                                       " freq=" + std::to_string(spec_.freq) +
                                                       " ch=" + std::to_string(spec_.channels));
}

auto SDL3SFXSource::EnsureStream() -> bool {
    if (stream_)
        return true;
    if (spec_.format == 0) {
        LOG_ERROR(atom::backend::sdl::LogChannel::AUDIO, "EnsureStream aborted: format=0 (spec not set)");
        return false;
    }

    LOG_DEBUG(atom::backend::sdl::LogChannel::AUDIO, "Opening stream: fmt=" + std::to_string(spec_.format) +
                                                       " freq=" + std::to_string(spec_.freq) +
                                                       " ch=" + std::to_string(spec_.channels));

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, nullptr, nullptr);
    if (!stream_) {
        LOG_ERROR(atom::backend::sdl::LogChannel::AUDIO,
                  "SDL_OpenAudioDeviceStream failed: " + std::string(SDL_GetError()));
        return false;
    }

    LOG_DEBUG(atom::backend::sdl::LogChannel::AUDIO, "Audio stream opened");
    return true;
}

auto SDL3SFXSource::SetBuffer(const uint8_t* data, uint32_t length) -> void {
    pcm_data_.assign(data, data + length);
    LOG_DEBUG(atom::backend::sdl::LogChannel::AUDIO, "SetBuffer: " + std::to_string(length) + " bytes");
}

auto SDL3SFXSource::Play() -> void {
    if (pcm_data_.empty()) {
        LOG_WARNING(atom::backend::sdl::LogChannel::AUDIO, "Play() called with empty data");
        return;
    }
    if (!EnsureStream()) {
        LOG_WARNING(atom::backend::sdl::LogChannel::AUDIO, "Play() aborted: cannot open stream");
        return;
    }

    SDL_ClearAudioStream(stream_);
    SDL_SetAudioStreamGain(stream_, volume_ / 100.0f);

    if (!SDL_PutAudioStreamData(stream_, pcm_data_.data(), static_cast<int>(pcm_data_.size()))) {
        LOG_ERROR(atom::backend::sdl::LogChannel::AUDIO, "SDL_PutAudioStreamData failed: " + std::string(SDL_GetError()));
        return;
    }
    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        LOG_ERROR(atom::backend::sdl::LogChannel::AUDIO,
                  "SDL_ResumeAudioStreamDevice failed: " + std::string(SDL_GetError()));
        return;
    }

    state_ = AudioSourceState::Playing;
    LOG_DEBUG(atom::backend::sdl::LogChannel::AUDIO, "Playback started: " + std::to_string(pcm_data_.size()) + " bytes");
}

auto SDL3SFXSource::Stop() -> void {
    if (!stream_)
        return;
    SDL_ClearAudioStream(stream_);
    SDL_PauseAudioStreamDevice(stream_);
    state_ = AudioSourceState::Stopped;
    LOG_DEBUG(atom::backend::sdl::LogChannel::AUDIO, "Playback stopped");
}

auto SDL3SFXSource::Pause() -> void {
    if (!stream_)
        return;
    SDL_PauseAudioStreamDevice(stream_);
    state_ = AudioSourceState::Paused;
    LOG_DEBUG(atom::backend::sdl::LogChannel::AUDIO, "Playback paused");
}

auto SDL3SFXSource::GetState() const -> AudioSourceState {
    return state_;
}

auto SDL3SFXSource::SetVolume(float volume) -> void {
    volume_ = volume;
    if (stream_) {
        SDL_SetAudioStreamGain(stream_, volume_ / 100.0f);
    }
}

auto SDL3SFXSource::GetVolume() const -> float {
    return volume_;
}

auto SDL3SFXSource::SetLooping(bool loop) -> void {
    looping_ = loop;
}

auto SDL3SFXSource::IsLooping() const -> bool {
    return looping_;
}

auto SDL3SFXSource::SetPlayingOffset(float) -> void {}
auto SDL3SFXSource::GetPlayingOffset() const -> float {
    return 0.0f;
}

auto SDL3SFXSource::IsFinished() const -> bool {
    if (state_ != AudioSourceState::Playing)
        return true;
    if (!stream_)
        return true;
    const bool dry = (SDL_GetAudioStreamAvailable(stream_) == 0);
    if (dry) {
        LOG_DEBUG(atom::audio::LogChannel::SFX, "Voice finished (stream dry)");
    }
    return dry;
}

} // namespace atom

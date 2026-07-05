#include "SDL3SFXSource.hpp"

#include <SDL3/SDL.h>

namespace atom {

SDL3SFXSource::SDL3SFXSource() = default;

SDL3SFXSource::~SDL3SFXSource() {
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
    }
}

auto SDL3SFXSource::SetSpec(const SDL_AudioSpec& spec) -> void {
    spec_ = spec;
}

auto SDL3SFXSource::EnsureStream() -> bool {
    if (stream_) return true;
    if (spec_.format == 0) return false;

    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, nullptr, nullptr);
    return stream_ != nullptr;
}

auto SDL3SFXSource::SetBuffer(const uint8_t* data, uint32_t length) -> void {
    pcm_data_.assign(data, data + length);
}

auto SDL3SFXSource::Play() -> void {
    if (pcm_data_.empty() || !EnsureStream()) return;

    SDL_ClearAudioStream(stream_);
    SDL_SetAudioStreamGain(stream_, volume_ / 100.0f);
    SDL_PutAudioStreamData(stream_, pcm_data_.data(), pcm_data_.size());
    SDL_ResumeAudioStreamDevice(stream_);
    state_ = AudioSourceState::Playing;
}

auto SDL3SFXSource::Stop() -> void {
    if (!stream_) return;
    SDL_ClearAudioStream(stream_);
    SDL_PauseAudioStreamDevice(stream_);
    state_ = AudioSourceState::Stopped;
}

auto SDL3SFXSource::Pause() -> void {
    if (!stream_) return;
    SDL_PauseAudioStreamDevice(stream_);
    state_ = AudioSourceState::Paused;
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

} // namespace atom

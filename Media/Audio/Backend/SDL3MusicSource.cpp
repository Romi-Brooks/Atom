#include "SDL3MusicSource.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>

namespace atom {

constexpr std::size_t kStreamChunk = 4096;

SDL3MusicSource::SDL3MusicSource(std::vector<uint8_t> pcmData,
                                 const SDL_AudioSpec& spec)
    : pcm_data_(std::move(pcmData))
    , spec_(spec)
{
}

auto SDL3MusicSource::EnsureStream() -> bool {
    if (stream_) return true;
    if (pcm_data_.empty()) return false;

    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, nullptr, nullptr);
    return stream_ != nullptr;
}

SDL3MusicSource::~SDL3MusicSource() {
    Stop();
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
    }
}

auto SDL3MusicSource::Play() -> void {
    if (!EnsureStream() || pcm_data_.empty()) return;

    AudioSourceState expected = AudioSourceState::Stopped;
    if (!state_.compare_exchange_strong(expected, AudioSourceState::Playing)) {
        return;
    }

    play_cursor_ = 0;
    thread_running_ = true;
    decode_thread_ = std::thread(&SDL3MusicSource::DecodeLoop, this);

    SDL_ResumeAudioStreamDevice(stream_);
}

auto SDL3MusicSource::Stop() -> void {
    state_.store(AudioSourceState::Stopped);
    thread_running_ = false;

    // Clear the stream *before* joining the decode thread to unblock any
    // pending SDL_PutAudioStreamData call. Otherwise the decode thread
    // hangs if the stream buffer is full, and join() deadlocks.
    if (stream_) {
        SDL_ClearAudioStream(stream_);
        SDL_PauseAudioStreamDevice(stream_);
    }

    if (decode_thread_.joinable()) {
        decode_thread_.join();
    }

    play_cursor_ = 0;
}

auto SDL3MusicSource::Pause() -> void {
    state_.store(AudioSourceState::Paused);
    if (stream_) {
        SDL_PauseAudioStreamDevice(stream_);
    }
}

auto SDL3MusicSource::GetState() const -> AudioSourceState {
    return state_.load();
}

auto SDL3MusicSource::SetVolume(float volume) -> void {
    volume_.store(volume);
    if (stream_) {
        SDL_SetAudioStreamGain(stream_, volume / 100.0f);
    }
}

auto SDL3MusicSource::GetVolume() const -> float {
    return volume_.load();
}

auto SDL3MusicSource::SetLooping(bool loop) -> void {
    loop_.store(loop);
}

auto SDL3MusicSource::IsLooping() const -> bool {
    return loop_.load();
}

auto SDL3MusicSource::SetPlayingOffset(float seconds) -> void {
    if (spec_.freq == 0) return;
    const auto totalFrames = pcm_data_.size() /
        (SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels);
    const auto targetFrame = static_cast<uint64_t>(seconds * spec_.freq);
    play_cursor_ = (std::min)(targetFrame, totalFrames) *
        SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels;
}

auto SDL3MusicSource::GetPlayingOffset() const -> float {
    if (spec_.freq == 0) return 0.0f;
    const auto bytesPerFrame = SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels;
    if (bytesPerFrame == 0) return 0.0f;
    return static_cast<float>(play_cursor_.load()) /
           (static_cast<float>(spec_.freq) * bytesPerFrame);
}

auto SDL3MusicSource::DecodeLoop() -> void {
    SDL_SetAudioStreamGain(stream_, volume_.load() / 100.0f);
    SDL_ResumeAudioStreamDevice(stream_);

    while (thread_running_.load() &&
           state_.load() == AudioSourceState::Playing) {

        auto cursor = play_cursor_.load();
        if (cursor < pcm_data_.size()) {
            const auto remaining = pcm_data_.size() - cursor;
            const auto toPush = (std::min)(kStreamChunk, remaining);

            SDL_PutAudioStreamData(stream_, pcm_data_.data() + cursor, toPush);
            play_cursor_.store(cursor + toPush);
        } else {
            if (loop_.load()) {
                play_cursor_ = 0;
                continue;
            }
            while (SDL_GetAudioStreamAvailable(stream_) > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            state_.store(AudioSourceState::Stopped);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    thread_running_ = false;
}

} // namespace atom

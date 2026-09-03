#include "SDL3MusicSource.hpp"

#include <algorithm>
#include <chrono>

#include <SDL3/SDL.h>

#include <Log/LogSystem.hpp>

namespace atom::backend::sdl3 {

// Target ~100 ms of audio per push — large enough to amortise resampler
// boundary artifacts but small enough to keep latency reasonable.
constexpr double kStreamDuration = 0.1; // seconds

SDL3MusicSource::SDL3MusicSource(std::vector<uint8_t> pcmData, const SDL_AudioSpec& spec)
    : pcm_data_(std::move(pcmData)), spec_(spec) {}

auto SDL3MusicSource::EnsureStream() -> bool {
    if (stream_)
        return true;
    if (pcm_data_.empty()) {
        LOG_WARNING(atom::audio::LogChannel::MUSIC, "EnsureStream: no PCM data loaded");
        return false;
    }

    LOG_DEBUG(atom::backend::sdl3::LogChannel::AUDIO,
              "Opening stream: fmt=" + std::to_string(spec_.format) + " freq=" + std::to_string(spec_.freq) +
                  " ch=" + std::to_string(spec_.channels) + " data_bytes=" + std::to_string(pcm_data_.size()));

    // Open stream bound to the default playback device, passing the source
    // format directly.  The decode thread pushes data in ~100 ms chunks so
    // that resampler boundary artifacts (the root cause of the 48000 Hz
    // crackling) are minimised.
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, nullptr, nullptr);
    if (!stream_) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::AUDIO,
                  "Failed to open audio stream: " + std::string(SDL_GetError()));
        return false;
    }

    LOG_DEBUG(atom::backend::sdl3::LogChannel::AUDIO, "Audio stream opened successfully");
    return true;
}

SDL3MusicSource::~SDL3MusicSource() {
    Stop();
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
    }
}

auto SDL3MusicSource::Play() -> void {
    if (pcm_data_.empty()) {
        LOG_WARNING(atom::audio::LogChannel::MUSIC, "Play() called with no data");
        return;
    }
    if (!EnsureStream()) {
        LOG_WARNING(atom::audio::LogChannel::MUSIC, "Play() aborted: cannot open stream");
        return;
    }

    atom::audio::AudioSourceState expected = atom::audio::AudioSourceState::Stopped;
    if (!state_.compare_exchange_strong(expected, atom::audio::AudioSourceState::Playing)) {
        LOG_DEBUG(atom::audio::LogChannel::MUSIC,
                  "Play() ignored: state is " + std::to_string(static_cast<int>(state_.load())));
        return;
    }

    play_cursor_ = 0;
    thread_running_ = true;
    decode_thread_ = std::thread(&SDL3MusicSource::DecodeLoop, this);

    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        LOG_ERROR(atom::audio::LogChannel::MUSIC, "SDL_ResumeAudioStreamDevice failed: " + std::string(SDL_GetError()));
    }
    LOG_INFO(atom::audio::LogChannel::MUSIC, "Playback started");
}

auto SDL3MusicSource::Stop() -> void {
    LOG_DEBUG(atom::audio::LogChannel::MUSIC, "Stop requested");
    state_.store(atom::audio::AudioSourceState::Stopped);
    thread_running_ = false;

    // Clear the stream *before* joining the decode thread to unblock any
    // pending SDL_PutAudioStreamData call.  Otherwise the decode thread
    // hangs if the stream buffer is full, and join() deadlocks.
    if (stream_) {
        SDL_ClearAudioStream(stream_);
        SDL_PauseAudioStreamDevice(stream_);
    }

    if (decode_thread_.joinable()) {
        decode_thread_.join();
    }

    play_cursor_ = 0;
    LOG_INFO(atom::audio::LogChannel::MUSIC, "Playback stopped");
}

auto SDL3MusicSource::Pause() -> void {
    state_.store(atom::audio::AudioSourceState::Paused);
    if (stream_) {
        SDL_PauseAudioStreamDevice(stream_);
    }
    LOG_DEBUG(atom::audio::LogChannel::MUSIC, "Playback paused");
}

auto SDL3MusicSource::GetState() const -> atom::audio::AudioSourceState {
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
    if (spec_.freq == 0)
        return;
    const auto totalFrames = pcm_data_.size() / (SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels);
    const auto targetFrame = static_cast<uint64_t>(seconds * spec_.freq);
    play_cursor_ = (std::min)(targetFrame, totalFrames) * SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels;
}

auto SDL3MusicSource::GetPlayingOffset() const -> float {
    if (spec_.freq == 0)
        return 0.0f;
    const auto bytesPerFrame = SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels;
    if (bytesPerFrame == 0)
        return 0.0f;
    return static_cast<float>(play_cursor_.load()) / (static_cast<float>(spec_.freq) * bytesPerFrame);
}

auto SDL3MusicSource::DecodeLoop() -> void {
    SDL_SetAudioStreamGain(stream_, volume_.load() / 100.0f);
    SDL_ResumeAudioStreamDevice(stream_);

    // Compute a reasonable push size (~100 ms of audio) so resampler
    // boundary artifacts are minimised.  Clamp to [4096, 1 MiB].
    const auto bytes_per_frame = SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels;
    const auto chunk_frames = static_cast<std::size_t>(spec_.freq * kStreamDuration);
    const std::size_t kStreamChunk =
        std::clamp(chunk_frames * bytes_per_frame, std::size_t{4096}, std::size_t{1048576});

    LOG_DEBUG(atom::backend::sdl3::LogChannel::AUDIO, "DecodeLoop started: chunk=" + std::to_string(kStreamChunk) +
                                                          " total=" + std::to_string(pcm_data_.size()) +
                                                          " loop=" + std::to_string(loop_.load()));

    auto last_debug_log = std::chrono::steady_clock::now();

    while (thread_running_.load() && state_.load() == atom::audio::AudioSourceState::Playing) {

        auto cursor = play_cursor_.load();
        if (cursor < pcm_data_.size()) {
            const auto remaining = pcm_data_.size() - cursor;
            const auto toPush = (std::min)(kStreamChunk, remaining);

            if (!SDL_PutAudioStreamData(stream_, pcm_data_.data() + cursor, static_cast<int>(toPush))) {
                LOG_ERROR(atom::backend::sdl3::LogChannel::AUDIO,
                          "SDL_PutAudioStreamData failed: " + std::string(SDL_GetError()));
                break;
            }
            play_cursor_.store(cursor + toPush);

            // Throttled debug: log push progress every 3 seconds
            const auto now = std::chrono::steady_clock::now();
            if (now - last_debug_log >= std::chrono::seconds(3)) {
                LOG_DEBUG(atom::backend::sdl3::LogChannel::AUDIO,
                          "Pushed " + std::to_string(toPush) + " bytes, cursor=" + std::to_string(cursor + toPush) +
                              "/" + std::to_string(pcm_data_.size()));
                last_debug_log = now;
            }
        } else {
            if (loop_.load()) {
                play_cursor_ = 0;
                LOG_DEBUG(atom::backend::sdl3::LogChannel::AUDIO, "Looping: rewound cursor to 0");
                continue;
            }
            // Wait for the stream to drain before stopping
            while (SDL_GetAudioStreamAvailable(stream_) > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            state_.store(atom::audio::AudioSourceState::Stopped);
            LOG_INFO(atom::audio::LogChannel::MUSIC, "Playback completed (end of data)");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    thread_running_ = false;
    LOG_DEBUG(atom::backend::sdl3::LogChannel::AUDIO, "DecodeLoop exited");
}

} // namespace atom::backend::sdl3

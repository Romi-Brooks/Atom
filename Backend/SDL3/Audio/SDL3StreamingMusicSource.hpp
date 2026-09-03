#ifndef ATOM_SDL3_STREAMING_MUSIC_SOURCE_HPP
#define ATOM_SDL3_STREAMING_MUSIC_SOURCE_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Audio/IAudioSource.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom::backend::sdl3 {

/// Streaming music source that decodes audio on-the-fly via atom::audio::IAudioDecoder.
///
/// Unlike SDL3MusicSource which holds the full decoded PCM in memory,
/// this source owns an opened decoder and uses a fixed-capacity ring buffer
/// to stream chunks from disk during playback.
///
/// Ring buffer: fixed ~4 MiB circular buffer with separate read/write cursors.
/// The decode thread fills the buffer from the decoder and a separate push
/// path drains it into the SDL audio stream.
class SDL3StreamingMusicSource final : public atom::audio::IAudioSource {
    public:
        SDL3StreamingMusicSource(std::unique_ptr<atom::audio::IAudioDecoder> decoder, const SDL_AudioSpec& spec);
        ~SDL3StreamingMusicSource() override;

        SDL3StreamingMusicSource(const SDL3StreamingMusicSource&) = delete;
        auto operator=(const SDL3StreamingMusicSource&) -> SDL3StreamingMusicSource& = delete;

        auto Play() -> void override;
        auto Stop() -> void override;
        auto Pause() -> void override;
        [[nodiscard]] auto GetState() const -> atom::audio::AudioSourceState override;
        auto SetVolume(float volume) -> void override;
        [[nodiscard]] auto GetVolume() const -> float override;
        auto SetLooping(bool loop) -> void override;
        [[nodiscard]] auto IsLooping() const -> bool override;
        auto SetPlayingOffset(float seconds) -> void override;
        [[nodiscard]] auto GetPlayingOffset() const -> float override;

    private:
        static constexpr std::size_t kRingBufferCapacity = 4 * 1024 * 1024; // 4 MiB
        // Target ~100 ms of audio per push (same as SDL3MusicSource).
        static constexpr double kChunkDuration = 0.1;
        static constexpr double kRingHighWaterDuration = 1.0;
        static constexpr double kSDLQueueTargetDuration = 0.2;
        // Minimum buffered data before pushing to SDL (2× chunk size).
        // Debug progress log cadence: one line per ~5 s of audio submitted,
        // regardless of wall-clock timing.
        static constexpr double kProgressLogIntervalSeconds = 5.0;

        SDL_AudioStream* stream_ = nullptr;
        SDL_AudioSpec spec_{};
        std::unique_ptr<atom::audio::IAudioDecoder> decoder_;

        // Ring buffer: indices increase monotonically; array access uses % capacity.
        std::vector<uint8_t> ring_buffer_;
        std::atomic<std::size_t> read_idx_{0};
        std::atomic<std::size_t> write_idx_{0};

        std::atomic<atom::audio::AudioSourceState> state_{atom::audio::AudioSourceState::Stopped};
        std::atomic<float> volume_{100.0f};
        std::atomic<bool> loop_{false};
        std::atomic<std::uint64_t> frames_submitted_{0};
        // Written only by the decode thread; reset while that thread is joined.
        std::uint64_t progress_logged_frames_ = 0;

        std::thread decode_thread_;
        std::atomic<bool> thread_running_{false};
        std::atomic<bool> eof_{false};
        std::atomic<bool> decode_error_{false};

        [[nodiscard]] auto ReadableBytes() const -> std::size_t;
        [[nodiscard]] auto WritableBytes() const -> std::size_t;

        auto EnsureStream() -> bool;
        auto DecodeLoop() -> void;
};

} // namespace atom::backend::sdl3

#endif // ATOM_SDL3_STREAMING_MUSIC_SOURCE_HPP

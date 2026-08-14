#include "SDL3StreamingMusicSource.hpp"

#include <algorithm>
#include <chrono>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Log/LogSystem.hpp>

namespace atom {

SDL3StreamingMusicSource::SDL3StreamingMusicSource(std::unique_ptr<IAudioDecoder> decoder, const SDL_AudioSpec& spec)
    : spec_(spec), decoder_(std::move(decoder)) {
    ring_buffer_.resize(kRingBufferCapacity);
}

SDL3StreamingMusicSource::~SDL3StreamingMusicSource() {
    Stop();
    if (stream_)
        SDL_DestroyAudioStream(stream_);
    if (decoder_)
        decoder_->Close();
}

auto SDL3StreamingMusicSource::ReadableBytes() const -> std::size_t {
    return write_idx_.load() - read_idx_.load();
}

auto SDL3StreamingMusicSource::WritableBytes() const -> std::size_t {
    const auto used = ReadableBytes();
    return used < kRingBufferCapacity ? kRingBufferCapacity - used : 0;
}

auto SDL3StreamingMusicSource::EnsureStream() -> bool {
    if (stream_)
        return true;
    if (!decoder_ || !decoder_->IsOpen()) {
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "EnsureStream: no valid decoder");
        return false;
    }

    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "Opening streaming stream: fmt=" + std::to_string(spec_.format) +
                                                       " freq=" + std::to_string(spec_.freq) +
                                                       " ch=" + std::to_string(spec_.channels));

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, nullptr, nullptr);
    if (!stream_) {
        LOG_ERROR(atom::LogChannel::SDL_BACKEND_AUDIO, "Failed to open audio stream: " + std::string(SDL_GetError()));
        return false;
    }

    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "Streaming audio stream opened successfully");
    return true;
}

auto SDL3StreamingMusicSource::Play() -> void {
    if (!decoder_ || !decoder_->IsOpen()) {
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "Play() called with no valid decoder");
        return;
    }
    if (!EnsureStream()) {
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "Play() aborted: cannot open stream");
        return;
    }

    if (state_.load() == AudioSourceState::Paused) {
        state_ = AudioSourceState::Playing;
        SDL_ResumeAudioStreamDevice(stream_);
        if (!thread_running_.exchange(true)) {
            if (decode_thread_.joinable())
                decode_thread_.join();
            decode_thread_ = std::thread(&SDL3StreamingMusicSource::DecodeLoop, this);
        }
        return;
    }

    AudioSourceState expected = AudioSourceState::Stopped;
    if (!state_.compare_exchange_strong(expected, AudioSourceState::Playing)) {
        LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_MUSIC,
                  "Play() ignored: state is " + std::to_string(static_cast<int>(state_.load())));
        return;
    }

    // Reset ring buffer and decoder state
    read_idx_ = 0;
    write_idx_ = 0;
    if (decode_thread_.joinable())
        decode_thread_.join();
    SDL_ClearAudioStream(stream_);
    frames_submitted_ = 0;
    eof_ = false;
    decode_error_ = false;

    if (!decoder_->Rewind()) {
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_MUSIC, "Play(): decoder rewind failed");
    }

    thread_running_ = true;
    decode_thread_ = std::thread(&SDL3StreamingMusicSource::DecodeLoop, this);

    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_MUSIC,
                  "SDL_ResumeAudioStreamDevice failed: " + std::string(SDL_GetError()));
    }
    LOG_INFO(atom::LogChannel::ATOM_AUDIO_MUSIC, "Streaming playback started");
}

auto SDL3StreamingMusicSource::Stop() -> void {
    LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_MUSIC, "Stop requested");
    state_.store(AudioSourceState::Stopped);
    thread_running_ = false;

    if (stream_) {
        SDL_ClearAudioStream(stream_);
        SDL_PauseAudioStreamDevice(stream_);
    }

    if (decode_thread_.joinable()) {
        decode_thread_.join();
    }

    read_idx_ = 0;
    write_idx_ = 0;
    LOG_INFO(atom::LogChannel::ATOM_AUDIO_MUSIC, "Playback stopped");
}

auto SDL3StreamingMusicSource::Pause() -> void {
    AudioSourceState expected = AudioSourceState::Playing;
    if (!state_.compare_exchange_strong(expected, AudioSourceState::Paused))
        return;
    thread_running_ = false;
    if (stream_) {
        SDL_PauseAudioStreamDevice(stream_);
    }
    LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_MUSIC, "Playback paused");
}

auto SDL3StreamingMusicSource::GetState() const -> AudioSourceState {
    return state_.load();
}

auto SDL3StreamingMusicSource::SetVolume(float volume) -> void {
    volume_.store(volume);
    if (stream_) {
        SDL_SetAudioStreamGain(stream_, volume / 100.0f);
    }
}

auto SDL3StreamingMusicSource::GetVolume() const -> float {
    return volume_.load();
}

auto SDL3StreamingMusicSource::SetLooping(bool loop) -> void {
    loop_.store(loop);
}

auto SDL3StreamingMusicSource::IsLooping() const -> bool {
    return loop_.load();
}

auto SDL3StreamingMusicSource::SetPlayingOffset(float seconds) -> void {
    if (!decoder_)
        return;
    const auto& info = decoder_->GetInfo();
    if (info.sample_rate == 0)
        return;
    const auto target_frame = static_cast<std::uint64_t>(seconds * static_cast<float>(info.sample_rate));
    // The decoder contract currently only guarantees rewind. Do not touch it
    // while the decode thread is active.
    if (target_frame != 0 || state_.load() == AudioSourceState::Playing)
        return;
    if (!decoder_->Rewind())
        return;
    if (stream_)
        SDL_ClearAudioStream(stream_);
    frames_submitted_ = 0;
    read_idx_ = 0;
    write_idx_ = 0;
    eof_ = false;
    decode_error_ = false;
}

auto SDL3StreamingMusicSource::GetPlayingOffset() const -> float {
    if (!decoder_)
        return 0.0f;
    const auto& info = decoder_->GetInfo();
    if (info.sample_rate == 0)
        return 0.0f;
    const auto bytes_per_frame = SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels;
    auto played_frames = frames_submitted_.load();
    if (stream_ && bytes_per_frame > 0) {
        const int queued = SDL_GetAudioStreamQueued(stream_);
        if (queued > 0) {
            const auto queued_frames = static_cast<std::uint64_t>(queued) / bytes_per_frame;
            played_frames = queued_frames < played_frames ? played_frames - queued_frames : 0;
        }
    }
    return static_cast<float>(played_frames) / static_cast<float>(info.sample_rate);
}

auto SDL3StreamingMusicSource::DecodeLoop() -> void {
    SDL_SetAudioStreamGain(stream_, volume_.load() / 100.0f);
    SDL_ResumeAudioStreamDevice(stream_);

    const auto bytes_per_frame = SDL_AUDIO_BYTESIZE(spec_.format) * spec_.channels;
    const auto chunk_frames = static_cast<std::size_t>(spec_.freq * kChunkDuration);
    const std::size_t stream_chunk =
        std::clamp(chunk_frames * bytes_per_frame, std::size_t{4096}, std::size_t{1048576});
    const auto bytes_per_second = static_cast<std::size_t>(spec_.freq) * bytes_per_frame;
    const auto watermark =
        std::min(kRingBufferCapacity, static_cast<std::size_t>(bytes_per_second * kRingHighWaterDuration));
    const auto sdl_queue_target =
        std::max(stream_chunk, static_cast<std::size_t>(bytes_per_second * kSDLQueueTargetDuration));

    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO,
              "DecodeLoop (streaming) started: chunk=" + std::to_string(stream_chunk) +
                  " watermark=" + std::to_string(watermark) + " loop=" + std::to_string(loop_.load()));

    while (thread_running_.load() && state_.load() == AudioSourceState::Playing) {

        // ---- Phase 1: decode data into ring buffer (producer) ----
        if (!eof_.load() && !decode_error_.load() && ReadableBytes() < watermark) {
            const auto writable = WritableBytes();
            if (writable >= stream_chunk) {
                const auto wi = write_idx_.load();
                auto* dst = ring_buffer_.data() + (wi % kRingBufferCapacity);
                const auto contiguous = kRingBufferCapacity - (wi % kRingBufferCapacity);
                const auto to_decode = std::min({writable, contiguous, stream_chunk});

                const auto decoded = decoder_->DecodeChunk(dst, static_cast<std::uint32_t>(to_decode));
                if (decoded > 0) {
                    write_idx_.store(wi + decoded);
                } else {
                    // 0 bytes returned = EOF or error
                    if (!decoder_->IsOpen()) {
                        decode_error_ = true;
                        LOG_ERROR(atom::LogChannel::SDL_BACKEND_AUDIO, "Decoder error in DecodeLoop");
                    } else {
                        eof_ = true;
                        LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "Decoder EOF reached");
                    }
                }
            }
        }

        // ---- Phase 2: push data from ring buffer to SDL stream (consumer) ----
        const auto readable = ReadableBytes();
        const int queued_result = SDL_GetAudioStreamQueued(stream_);
        if (queued_result < 0) {
            decode_error_ = true;
            LOG_ERROR(atom::LogChannel::SDL_BACKEND_AUDIO,
                      "SDL_GetAudioStreamQueued failed: " + std::string(SDL_GetError()));
        }
        const auto queued = queued_result > 0 ? static_cast<std::size_t>(queued_result) : 0;
        if (readable > 0 && queued < sdl_queue_target) {
            const auto ri = read_idx_.load();
            auto* src = ring_buffer_.data() + (ri % kRingBufferCapacity);
            const auto contiguous = kRingBufferCapacity - (ri % kRingBufferCapacity);
            auto to_push = std::min({readable, contiguous, stream_chunk});
            if (eof_.load()) {
                // At EOF, push whatever remains — don't wait for a full chunk.
                to_push = std::min({readable, contiguous});
            }

            if (!SDL_PutAudioStreamData(stream_, src, static_cast<int>(to_push))) {
                LOG_ERROR(atom::LogChannel::SDL_BACKEND_AUDIO,
                          "SDL_PutAudioStreamData failed: " + std::string(SDL_GetError()));
                break;
            }
            read_idx_.store(ri + to_push);
            frames_submitted_.fetch_add(to_push / bytes_per_frame);
        }

        // ---- Phase 3: handle EOF / loop / stop ----
        if (eof_.load() && ReadableBytes() == 0 && SDL_GetAudioStreamQueued(stream_) == 0) {
            if (loop_.load()) {
                LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "Looping: rewinding decoder");
                if (decoder_->Rewind()) {
                    eof_ = false;
                    read_idx_ = 0;
                    write_idx_ = 0;
                    frames_submitted_ = 0;
                    continue;
                }
                LOG_ERROR(atom::LogChannel::SDL_BACKEND_AUDIO, "Decoder rewind failed during loop");
            }
            // Drain SDL stream before stopping
            while (SDL_GetAudioStreamAvailable(stream_) > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            state_.store(AudioSourceState::Stopped);
            LOG_INFO(atom::LogChannel::ATOM_AUDIO_MUSIC, "Playback completed (end of data)");
            break;
        }

        // ---- Phase 4: handle decode error ----
        if (decode_error_.load() && ReadableBytes() == 0) {
            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_MUSIC, "Playback stopped due to decode error");
            state_.store(AudioSourceState::Stopped);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    thread_running_ = false;
    LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "DecodeLoop (streaming) exited");
}

} // namespace atom

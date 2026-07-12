#ifndef ATOM_SDL3_MUSIC_SOURCE_HPP
#define ATOM_SDL3_MUSIC_SOURCE_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include <Engine/Interfaces/IAudioSource.hpp>

namespace atom {

class SDL3MusicSource : public IAudioSource {
public:
    explicit SDL3MusicSource(std::vector<uint8_t> pcmData,
                             const SDL_AudioSpec& spec);
    ~SDL3MusicSource() override;

    SDL3MusicSource(const SDL3MusicSource&) = delete;
    auto operator=(const SDL3MusicSource&) -> SDL3MusicSource& = delete;

    auto Play() -> void override;
    auto Stop() -> void override;
    auto Pause() -> void override;
    [[nodiscard]] auto GetState() const -> AudioSourceState override;
    auto SetVolume(float volume) -> void override;
    [[nodiscard]] auto GetVolume() const -> float override;
    auto SetLooping(bool loop) -> void override;
    [[nodiscard]] auto IsLooping() const -> bool override;
    auto SetPlayingOffset(float seconds) -> void override;
    [[nodiscard]] auto GetPlayingOffset() const -> float override;

private:
    SDL_AudioStream* stream_ = nullptr;

    std::vector<uint8_t> pcm_data_;
    SDL_AudioSpec spec_{};

    std::atomic<AudioSourceState> state_{AudioSourceState::Stopped};
    std::atomic<float> volume_{100.0f};
    std::atomic<bool> loop_{false};
    std::atomic<uint64_t> play_cursor_{0};

    std::thread decode_thread_;
    std::atomic<bool> thread_running_{false};

    auto EnsureStream() -> bool;
    auto DecodeLoop() -> void;
};

} // namespace atom

#endif // ATOM_SDL3_MUSIC_SOURCE_HPP

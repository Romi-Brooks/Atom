#ifndef ATOM_MUSIC_PLAYER_HPP
#define ATOM_MUSIC_PLAYER_HPP

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioSource.hpp>
#include <Backend/Runtime/IAudioBackendChangeListener.hpp>

namespace atom {
class AudioDecoderRegistry;
class AudioMixer;
class BackendRuntime;
class IAudioBackend;

class MusicPlayer final : public IAudioBackendChangeListener {
public:
    explicit MusicPlayer(AudioMixer& mixer);
    MusicPlayer(IAudioBackend& backend, AudioDecoderRegistry& decoders, AudioMixer& mixer);
    ~MusicPlayer() override;

    auto Load(const std::string& id, const std::string& file) -> bool;
    auto Play(const std::string& id) -> void;
    auto Play(const std::string& id, float volume) -> void;
    auto Stop(const std::string& id) -> void;
    auto Reset() -> void;
    auto SetVolume(const std::string& id, float volume) -> void;
    auto SetMusicVolume(float volume) -> void;
    [[nodiscard]] auto GetMusicVolume() const -> float;
    auto SetNowPlaying(const std::string& id) -> void;
    [[nodiscard]] auto GetNowPlaying() const -> std::string;
    [[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;
    [[nodiscard]] auto IsNowPlaying(const std::string& id) const -> bool;
    auto ClearNowPlaying() -> void;

    auto OnAudioBackendChanging() -> void override;
    auto OnAudioDecoderBackendChanging() -> void override;

private:
    struct Track {
        std::unique_ptr<IAudioSource> source;
    };

    IAudioBackend* backend_;
    AudioDecoderRegistry* decoders_;
    AudioMixer& mixer_;
    BackendRuntime* runtime_ = nullptr;
    std::unordered_map<std::string, Track> tracks_;
    std::string current_playing_id_;
    mutable std::mutex mutex_;
};

using Music = MusicPlayer;
} // namespace atom

#endif

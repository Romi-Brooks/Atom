#ifndef ATOM_MUSIC_PLAYER_HPP
#define ATOM_MUSIC_PLAYER_HPP

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioSource.hpp>
#include <Backend/Runtime/IAudioBackendChangeListener.hpp>

namespace atom::audio {
class AudioDecoderRegistry;
class IAudioBackend;
} // namespace atom::audio

namespace atom::backend {
class BackendRuntime;
}

namespace atom {
class AudioMixer;

class MusicPlayer final : public atom::backend::IAudioBackendChangeListener {
    public:
        explicit MusicPlayer(AudioMixer& mixer);
        MusicPlayer(atom::audio::IAudioBackend& backend, atom::audio::AudioDecoderRegistry& decoders,
                    AudioMixer& mixer);
        ~MusicPlayer() override;

        auto Load(const std::string& id, const std::string& file) -> bool;

        // Load a music track from an in-memory buffer, e.g. an entry extracted
        // from a resource pack. filename is used only to select a decoder by
        // extension. The buffer is borrowed: the caller must keep it alive for as
        // long as the track is loaded (until Reset() or destruction).
        auto LoadFromMemory(const std::string& id, const std::string& filename, const void* data, std::size_t size)
            -> bool;

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

    private:
        struct Track {
                std::unique_ptr<atom::audio::IAudioSource> source;
        };

        atom::audio::IAudioBackend* backend_;
        atom::audio::AudioDecoderRegistry* decoders_;
        AudioMixer& mixer_;
        atom::backend::BackendRuntime* runtime_ = nullptr;
        std::unordered_map<std::string, Track> tracks_;
        std::string current_playing_id_;
        mutable std::mutex mutex_;
};

} // namespace atom

#endif

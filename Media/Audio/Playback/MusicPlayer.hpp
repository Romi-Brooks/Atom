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
#include <Filesystem/FileSystem.hpp>

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

        // Load a music track through the VFS. The path's extension selects the
        // decoder; the file is opened via the filesystem and streamed by the
        // decoder, so no native path is involved. The VFS IFile is owned by the
        // track for as long as it is loaded.
        auto Load(const std::string& id, const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path)
            -> bool;

        // Convenience overload: parses `path` as an AssetPath (e.g. "res://music/"
        // "song.mp3") and resolves it through the process-wide default Vfs
        // (atom::fs::Vfs::GetInstance()). Returns false when the string is not a
        // valid asset path. Prefer the IFileSystem overload when the filesystem
        // is already in hand; this exists so string-only callers (scripts, Lua,
        // quick tools) do not have to build an AssetPath themselves.
        auto Load(const std::string& id, const std::string& path) -> bool;

        // Load a music track from an in-memory buffer, e.g. an entry extracted
        // from a resource pack. filename is used only to select a decoder by
        // extension. The buffer is borrowed: the caller must keep it alive for as
        // long as the track is loaded (until Reset() or destruction).
        auto LoadFromMemory(const std::string& id, const std::string& filename, const void* data, std::size_t size)
            -> bool;

        auto Play(const std::string& id) -> void;
        auto Play(const std::string& id, float volume) -> void;
        auto Pause(const std::string& id) -> void;
        auto Stop(const std::string& id) -> void;
        [[nodiscard]] auto GetState(const std::string& id) const -> atom::audio::AudioSourceState;

        // Seek to a position measured in seconds from the start of the track.
        //
        // Music is decoded by the format decoder owned by the source (WavProf,
        // Minimp3Decoder, or a user-registered IAudioDecoder), so a seek requires
        // that decoder to implement IAudioDecoder::SeekToFrame. The request is
        // clamped to the known duration; seeking while playing continues playback
        // from the new position and may be audible as a short gap, because the
        // backend drops the audio it had already queued. Returns false when the id
        // is unknown or the position cannot be reached.
        auto Seek(const std::string& id, float seconds) -> bool;
        [[nodiscard]] auto GetPlayingOffset(const std::string& id) const -> float;
        // Total length in seconds, or 0 when the decoder could not report it
        // (e.g. a CBR MP3 without a VBR tag).
        [[nodiscard]] auto GetDuration(const std::string& id) const -> float;
        // Whether Seek() can reach arbitrary positions for this id.
        [[nodiscard]] auto IsSeekable(const std::string& id) const -> bool;

        auto Reset() -> void;
        auto SetVolume(const std::string& id, float volume) -> void;
        auto SetLooping(const std::string& id, bool loop) -> void;
        [[nodiscard]] auto IsLooping(const std::string& id) const -> bool;
        auto SetMusicVolume(float volume) -> void;
        [[nodiscard]] auto GetMusicVolume() const -> float;
        auto SetNowPlaying(const std::string& id) -> void;
        [[nodiscard]] auto GetNowPlaying() const -> std::string;
        [[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;
        [[nodiscard]] auto IsNowPlaying(const std::string& id) const -> bool;
        // Returns true only when the source reached EOF naturally. When this
        // happens, the stale now-playing id is cleared as part of the poll.
        [[nodiscard]] auto IsFinished(const std::string& id) const -> bool;
        auto ClearNowPlaying() -> void;

        auto OnAudioBackendChanging() -> void override;

    private:
        struct Track {
                std::unique_ptr<atom::audio::IAudioSource> source;
                // Seconds; 0 when the decoder could not report the length.
                float duration_seconds = 0.0f;
                // Owns the underlying VFS file when the track was opened from a
                // stream (Load(IFileSystem, AssetPath)). Must outlive `source`
                // (whose decoder reads from it); kept alongside the source and
                // destroyed after it. Null for the path and memory entry points.
                std::unique_ptr<atom::fs::IFile> file;
        };

        atom::audio::IAudioBackend* backend_;
        atom::audio::AudioDecoderRegistry* decoders_;
        AudioMixer& mixer_;
        atom::backend::BackendRuntime* runtime_ = nullptr;
        std::unordered_map<std::string, Track> tracks_;
        mutable std::string current_playing_id_;
        mutable std::mutex mutex_;

        auto RefreshNowPlayingLocked() const -> void;
};

} // namespace atom

#endif

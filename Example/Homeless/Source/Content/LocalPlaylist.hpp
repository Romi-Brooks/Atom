#ifndef ATOM_HOMELESS_LOCALPLAYLIST_HPP
#define ATOM_HOMELESS_LOCALPLAYLIST_HPP

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <Atom/Core/Concurrency/Job.hpp>
#include <Atom/Resources/Media/Audio/Playback/MusicPlayer.hpp>

namespace homeless {

struct LocalTrack {
        std::string uri;
        std::string title;
        std::string artist;
        bool unavailable = false;
};

class LocalPlaylist final {
    public:
        explicit LocalPlaylist(atom::AudioMixer& mixer) : music_(mixer) {}
        ~LocalPlaylist();

        auto Scan(const std::filesystem::path& directory) -> void;
        auto Update(float volume) -> void;
        auto Stop() -> void;
        auto Toggle() -> void;
        auto Move(int direction) -> void;

        [[nodiscard]] auto Selected() const -> const LocalTrack*;
        [[nodiscard]] auto CanMove() const -> bool {
            return playable_count_ > 1;
        }
        [[nodiscard]] auto WantsPlayback() const -> bool {
            return desired_.play;
        }
        [[nodiscard]] auto IsPlaying() const -> bool {
            return playing_;
        }
        [[nodiscard]] auto IsScanning() const -> bool {
            return scan_job_.IsValid();
        }

    private:
        struct Playback {
                std::size_t index = 0;
                float volume = 0.0f;
                bool play = false;
                auto operator==(const Playback&) const -> bool = default;
        };

        auto SkipUnavailable() -> void;
        auto StartPlaybackJob() -> void;

        atom::MusicPlayer music_;
        atom::job::JobHandle scan_job_{};
        atom::job::JobHandle audio_job_{};
        std::vector<LocalTrack> scanned_{};
        std::vector<LocalTrack> tracks_{};
        std::size_t playable_count_ = 0;
        Playback desired_{};
        std::optional<Playback> applied_{};
        Playback in_flight_{};
        std::atomic<std::size_t> revision_{0};
        std::optional<std::size_t> loaded_{}; // Accessed only by serial audio jobs.
        bool playback_failed_ = false;
        bool playback_cancelled_ = false;
        bool playing_ = false;
};

} // namespace homeless

#endif // ATOM_HOMELESS_LOCALPLAYLIST_HPP

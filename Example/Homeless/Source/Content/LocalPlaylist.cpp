#include "LocalPlaylist.hpp"

#include <algorithm>
#include <utility>

#include <Atom/Resources/Media/Audio/Resources/AudioDecoderRegistry.hpp>
#include <Atom/Core/Concurrency/JobSystem.hpp>
#include <Atom/Resources/Filesystem/Vfs.hpp>
#include <Atom/Core/Log/LogSystem.hpp>
#include <Atom/Resources/Media/Audio/AudioBackend.hpp>
#include <Atom/Core/Utilities/Utf8/Utf8.hpp>

namespace homeless {

namespace {

const std::string player_id = "phone";

auto DiscoverTracks(const std::filesystem::path& directory) -> std::vector<LocalTrack> {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec || atom::fs::Vfs::GetInstance().MountNative("localmusic", 200, atom::PathToUtf8(directory)) !=
                  atom::fs::Result::Success) {
        LOG_WARNING(atom::log::audio::Music, "Cannot open the Homeless Music folder");
        return {};
    }

    std::vector<LocalTrack> result;
    auto& decoders = atom::audio::GetAudioDecoders();
    std::filesystem::directory_iterator entry(directory, ec);
    for (const auto end = std::filesystem::directory_iterator{}; !ec && entry != end; entry.increment(ec)) {
        std::error_code status_error;
        if (!entry->is_regular_file(status_error))
            continue;
        const auto extension = atom::PathToUtf8(entry->path().extension());
        if (extension.empty() || !decoders.Contains(extension))
            continue;
        LocalTrack track{.uri = "localmusic://" + atom::PathToUtf8(entry->path().filename()),
                         .title = atom::PathToUtf8(entry->path().stem())};
        if (const auto separator = track.title.find(" - "); separator != std::string::npos) {
            track.artist = track.title.substr(0, separator);
            track.title.erase(0, separator + 3);
        }
        result.push_back(std::move(track));
    }
    if (ec)
        LOG_WARNING(atom::log::audio::Music, "Could not finish scanning the Homeless Music folder: " + ec.message());
    std::ranges::sort(result, {}, &LocalTrack::uri);
    LOG_INFO(atom::log::audio::Music, "Homeless Music folder: " + std::to_string(result.size()) + " playable file(s)");
    return result;
}

} // namespace

LocalPlaylist::~LocalPlaylist() {
    scan_job_.Wait();
    audio_job_.Wait();
}

auto LocalPlaylist::Scan(const std::filesystem::path& directory) -> void {
    scan_job_ = atom::job::JobSystem::GetInstance().Schedule(
        "Homeless: scan Music folder", [this, directory] { scanned_ = DiscoverTracks(directory); });
}

auto LocalPlaylist::Selected() const -> const LocalTrack* {
    return playable_count_ == 0 ? nullptr : &tracks_[desired_.index];
}

auto LocalPlaylist::Toggle() -> void {
    if (!Selected())
        return;
    desired_.play = !desired_.play;
    playing_ = false;
    ++revision_;
}

auto LocalPlaylist::Move(int direction) -> void {
    if (playable_count_ == 0)
        return;
    const auto original = desired_.index;
    do {
        desired_.index = direction < 0 ? (desired_.index == 0 ? tracks_.size() - 1 : desired_.index - 1)
                                       : (desired_.index + 1) % tracks_.size();
    } while (tracks_[desired_.index].unavailable);
    if (original != desired_.index) {
        playing_ = false;
        ++revision_;
    }
}

auto LocalPlaylist::SkipUnavailable() -> void {
    tracks_[desired_.index].unavailable = true;
    if (--playable_count_ != 0)
        Move(1);
    else
        desired_.play = false;
}

auto LocalPlaylist::StartPlaybackJob() -> void {
    in_flight_ = desired_;
    const auto revision = revision_.load();
    const auto uri = desired_.play ? tracks_[desired_.index].uri : std::string{};
    audio_job_ = atom::job::JobSystem::GetInstance().Schedule(
        "Homeless: music transport",
        [this, request = in_flight_, revision, uri] {
            playback_failed_ = false;
            playback_cancelled_ = revision != revision_.load();
            if (playback_cancelled_)
                return;
            if (loaded_ && *loaded_ != request.index) {
                (void)music_.Unload(player_id);
                loaded_.reset();
            }
            if (!request.play) {
                if (loaded_)
                    music_.Pause(player_id);
                return;
            }
            if (!loaded_) {
                if (!music_.Load(player_id, uri)) {
                    playback_failed_ = true;
                    return;
                }
                loaded_ = request.index;
            }
            playback_cancelled_ = revision != revision_.load();
            if (playback_cancelled_)
                return;
            if (music_.GetState(player_id) == atom::audio::AudioSourceState::Playing)
                music_.SetVolume(player_id, request.volume);
            else
                music_.Play(player_id, request.volume);
            playback_failed_ =
                music_.GetState(player_id) == atom::audio::AudioSourceState::Stopped && !music_.IsFinished(player_id);
        },
        atom::job::Priority::High);
}

auto LocalPlaylist::Update(float volume) -> void {
    desired_.volume = volume;
    if (scan_job_.IsValid() && scan_job_.IsComplete()) {
        scan_job_ = {};
        tracks_ = std::move(scanned_);
        playable_count_ = tracks_.size();
        applied_ = desired_;
    }
    if (audio_job_.IsValid()) {
        if (!audio_job_.IsComplete())
            return;
        audio_job_ = {};
        if (playback_cancelled_)
            applied_.reset();
        else
            applied_ = in_flight_;
        if (playback_failed_ && in_flight_.index == desired_.index)
            SkipUnavailable();
    }

    playing_ = false;
    if (!playable_count_)
        return;
    if (applied_ && applied_->index == desired_.index && applied_->play && desired_.play) {
        const auto state = music_.GetState(player_id);
        playing_ = state == atom::audio::AudioSourceState::Playing;
        if (state == atom::audio::AudioSourceState::Stopped && music_.IsFinished(player_id)) {
            Move(1);
            applied_.reset(); // At EOF, a one-track playlist starts the same track again.
        } else if (state == atom::audio::AudioSourceState::Stopped) {
            SkipUnavailable();
        }
    }
    if (playable_count_ && applied_ != desired_)
        StartPlaybackJob();
}

auto LocalPlaylist::Stop() -> void {
    desired_.play = false;
    ++revision_;
    std::vector<atom::job::JobHandle> dependencies;
    if (audio_job_.IsValid())
        dependencies.push_back(audio_job_);
    auto stop = atom::job::JobSystem::GetInstance().Schedule(
        "Homeless: stop local music",
        [this] {
            (void)music_.Unload(player_id);
            loaded_.reset();
            playback_failed_ = false;
            playback_cancelled_ = false;
        },
        atom::job::Priority::High, std::move(dependencies));
    if (stop.IsValid()) {
        audio_job_ = std::move(stop);
        in_flight_ = desired_;
    }
}

} // namespace homeless

#include "MusicPlayer.hpp"

#include <algorithm>
#include <vector>

#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Contracts/Numeric/NumericContract.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Backend/Runtime/BackendRuntime.hpp>
#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Resources/AudioClipLoader.hpp>

namespace atom {

namespace {
// Borrowed (non-owning) shared_ptr for an explicitly injected backend, so the
// load path can use one type whether the backend comes from the runtime or from
// the constructor.
auto BorrowBackend(atom::audio::IAudioBackend* backend) -> std::shared_ptr<atom::audio::IAudioBackend> {
    return std::shared_ptr<atom::audio::IAudioBackend>{backend, [](atom::audio::IAudioBackend*) {}};
}
} // namespace

MusicPlayer::MusicPlayer(AudioMixer& mixer)
    : backend_(nullptr), decoders_(&atom::backend::BackendRuntime::GetInstance().AudioDecoders()), mixer_(mixer),
      runtime_(&atom::backend::BackendRuntime::GetInstance()) {
    runtime_->AddAudioListener(*this);
}

MusicPlayer::MusicPlayer(atom::audio::IAudioBackend& backend, atom::audio::AudioDecoderRegistry& decoders,
                         AudioMixer& mixer)
    : backend_(&backend), decoders_(&decoders), mixer_(mixer) {}

MusicPlayer::~MusicPlayer() {
    if (runtime_)
        runtime_->RemoveAudioListener(*this);
}

auto MusicPlayer::Load(const std::string& id, const atom::fs::IFileSystem& filesystem,
                       const atom::fs::AssetPath& path) -> bool {
    if (FindTrack(id)) {
        LOG_DEBUG(atom::log::audio::Music, "Music track already loaded, skip: " + id);
        return true;
    }

    AudioClipLoader loader{*decoders_};
    const std::string label{path.String()};
    LOG_INFO(atom::log::audio::Music, "Initializing VFS music decoder on backend '" +
                                          (runtime_ ? runtime_->GetAudioBackendId() : std::string{"explicit"}) + "': " +
                                          label);
    auto backend = runtime_ ? runtime_->AcquireAudioBackend() : BorrowBackend(backend_);
    if (!backend) {
        LOG_ERROR(atom::log::audio::Music, "No active audio backend, cannot load music: " + label);
        return false;
    }
    auto streaming = loader.OpenStreaming(filesystem, path);
    if (!streaming) {
        // The loader's error names the category (no decoder / declined / I/O) and
        // the resource, which the old optional could not carry.
        LOG_ERROR(atom::log::audio::Music, "Failed to open music stream: " + streaming.error().Message());
        return false;
    }
    const auto& decoder_info = streaming->decoder->GetInfo();
    const auto duration_seconds =
        decoder_info.sample_rate > 0 && decoder_info.total_pcm_frames > 0
            ? static_cast<float>(decoder_info.total_pcm_frames) / static_cast<float>(decoder_info.sample_rate)
            : 0.0f;

    auto source = backend->CreateStreamingMusicSource(std::move(streaming->decoder), streaming->spec);
    if (!source) {
        LOG_ERROR(atom::log::audio::Music,
                  "Failed to create streaming music source for track '" + id + "': " + label);
        return false;
    }
    // The streaming source's decoder reads from the VFS file, so the file must
    // stay alive for as long as the track does. Track declares it first for
    // exactly that reason; it is filled in before the source so the ownership
    // order is visible at the construction site too.
    auto track = std::make_shared<Track>();
    track->file = std::move(streaming->file);
    track->source = std::move(source);
    track->duration_seconds = duration_seconds;
    {
        std::lock_guard lock(mutex_);
        // Another worker may have won a duplicate asynchronous load while this
        // decoder was opening. Keep the established source in that case.
        if (tracks_.contains(id)) {
            LOG_DEBUG(atom::log::audio::Music, "Music track already loaded, skip: " + id);
            return true;
        }
        tracks_.emplace(id, std::move(track));
    }
    LOG_INFO(atom::log::audio::Music, "Music track loaded via VFS: " + id + " (" + label + ")");
    return true;
}

auto MusicPlayer::Load(const std::string& id, const std::string& path) -> bool {
    auto asset_path = atom::fs::AssetPath::Parse(path);
    if (!asset_path) {
        LOG_ERROR(atom::log::audio::Music, "Invalid asset path for music track '" + id + "': " + path + " (" +
                                               asset_path.error().Message() + ")");
        return false;
    }
    return Load(id, atom::fs::Vfs::GetInstance(), *asset_path);
}

auto MusicPlayer::LoadFromMemory(const std::string& id, const std::string& filename, const void* data,
                                 const std::size_t size) -> bool {
    // ARCH-006: a null/short buffer with a non-zero size is a caller bug that the
    // decoders cannot survive; reject it before any decoder sees it.
    if (!atom::numeric::IsValidBuffer(data, size)) {
        LOG_ERROR(atom::log::audio::Music,
                  "LoadFromMemory rejected an empty buffer for music track '" + id + "' (" + filename + ")");
        return false;
    }

    if (FindTrack(id)) {
        LOG_DEBUG(atom::log::audio::Music, "Music track already loaded, skip: " + id);
        return true;
    }

    AudioClipLoader loader{*decoders_};
    LOG_INFO(atom::log::audio::Music, "Initializing in-memory music decoder on backend '" +
                                          (runtime_ ? runtime_->GetAudioBackendId() : std::string{"explicit"}) + "': " + filename);
    // Hold a strong reference for the whole load: it keeps the backend (and the
    // platform subsystem it leases) alive even if another thread switches
    // backends while this decoder is opening.
    auto backend = runtime_ ? runtime_->AcquireAudioBackend() : BorrowBackend(backend_);
    if (!backend) {
        LOG_ERROR(atom::log::audio::Music, "No active audio backend, cannot load music from memory: " + filename);
        return false;
    }
    auto streaming = loader.OpenStreamingFromMemory(filename, data, size);
    if (!streaming) {
        LOG_ERROR(atom::log::audio::Music, "Failed to decode music from memory: " + filename);
        return false;
    }
    const auto& decoder_info = streaming->decoder->GetInfo();
    const auto duration_seconds =
        decoder_info.sample_rate > 0 && decoder_info.total_pcm_frames > 0
            ? static_cast<float>(decoder_info.total_pcm_frames) / static_cast<float>(decoder_info.sample_rate)
            : 0.0f;

    auto source = backend->CreateStreamingMusicSource(std::move(streaming->decoder), streaming->spec);
    if (!source) {
        LOG_ERROR(atom::log::audio::Music,
                  "Failed to create streaming music source for track '" + id + "': " + filename);
        return false;
    }
    auto track = std::make_shared<Track>();
    track->source = std::move(source);
    track->duration_seconds = duration_seconds;
    {
        std::lock_guard lock(mutex_);
        if (tracks_.contains(id)) {
            LOG_DEBUG(atom::log::audio::Music, "Music track already loaded, skip: " + id);
            return true;
        }
        tracks_.emplace(id, std::move(track));
    }
    LOG_INFO(atom::log::audio::Music,
             "Music track loaded from memory: " + id + " (" + filename + ", " + std::to_string(size) + " bytes)");
    return true;
}

auto MusicPlayer::Play(const std::string& id) -> void {
    Play(id, mixer_.GetEffectiveMusicVolume());
}

auto MusicPlayer::Play(const std::string& id, const float volume) -> void {
    const auto track = FindTrack(id);
    if (!track || !track->source) {
        return;
    }
    std::lock_guard control_lock(track->control_mutex);
    if (track->source->GetState() == atom::audio::AudioSourceState::Playing) {
        LOG_DEBUG(atom::log::audio::Music, "Play() ignored: track is already playing: " + id);
        return;
    }
    track->source->SetVolume(atom::numeric::SanitizeVolume(volume));
    track->source->Play();
    const auto state = track->source->GetState();
    if (state != atom::audio::AudioSourceState::Playing && state != atom::audio::AudioSourceState::Paused) {
        LOG_WARNING(atom::log::audio::Music, "Play() did not start track: " + id);
        return;
    }
    LOG_INFO(atom::log::audio::Music, "Now Playing track: " + id);
    std::lock_guard lock(mutex_);
    current_playing_id_ = id;
}

auto MusicPlayer::Pause(const std::string& id) -> void {
    const auto track = FindTrack(id);
    if (!track || !track->source) {
        return;
    }
    std::lock_guard control_lock(track->control_mutex);
    if (track->source->GetState() != atom::audio::AudioSourceState::Playing) {
        LOG_DEBUG(atom::log::audio::Music, "Pause() ignored: track is not playing: " + id);
        return;
    }
    track->source->Pause();
    LOG_INFO(atom::log::audio::Music, "Track paused: " + id);
}

auto MusicPlayer::GetState(const std::string& id) const -> atom::audio::AudioSourceState {
    const auto track = FindTrack(id);
    if (!track || !track->source) {
        return atom::audio::AudioSourceState::Stopped;
    }
    return track->source->GetState();
}

auto MusicPlayer::Stop(const std::string& id) -> void {
    const auto track = FindTrack(id);
    if (!track || !track->source) {
        return;
    }
    std::lock_guard control_lock(track->control_mutex);
    const auto was_active = track->source->GetState() != atom::audio::AudioSourceState::Stopped;
    track->source->Stop();
    if (was_active) {
        LOG_INFO(atom::log::audio::Music, "Track Stopped: " + id);
    } else {
        LOG_DEBUG(atom::log::audio::Music, "Stop() ignored: track is already stopped: " + id);
    }
    std::lock_guard lock(mutex_);
    if (current_playing_id_ == id)
        current_playing_id_.clear();
}

auto MusicPlayer::Seek(const std::string& id, const float seconds) -> bool {
    // ARCH-006: never hand a NaN/infinite position to the backend. A
    // non-finite request is a failed seek, not a clamp to 0.
    float requested = 0.0f;
    if (!atom::numeric::TrySanitizeOffsetSeconds(seconds, requested)) {
        LOG_WARNING(atom::log::audio::Music, "Seek() rejected a non-finite position for track " + id);
        return false;
    }

    const auto track = FindTrack(id);
    if (!track || !track->source) {
        LOG_DEBUG(atom::log::audio::Music, "Seek() ignored: unknown track: " + id);
        return false;
    }
    auto target = requested;
    if (track->duration_seconds > 0.0f)
        target = std::min(target, track->duration_seconds);
    std::lock_guard control_lock(track->control_mutex);
    if (!track->source->SetPlayingOffset(target)) {
        LOG_WARNING(atom::log::audio::Music,
                    "Seek() failed on the active playback backend: track " + id + " at " +
                        std::to_string(static_cast<double>(target)) + "s");
        return false;
    }
    LOG_INFO(atom::log::audio::Music,
             "Track seeked: " + id + " -> " + std::to_string(static_cast<double>(target)) + "s");
    return true;
}

auto MusicPlayer::GetPlayingOffset(const std::string& id) const -> float {
    const auto track = FindTrack(id);
    if (!track || !track->source)
        return 0.0f;
    return track->source->GetPlayingOffset();
}

auto MusicPlayer::GetDuration(const std::string& id) const -> float {
    const auto track = FindTrack(id);
    return track ? track->duration_seconds : 0.0f;
}

auto MusicPlayer::IsSeekable(const std::string& id) const -> bool {
    const auto track = FindTrack(id);
    return track && track->source && track->source->IsSeekable();
}

auto MusicPlayer::Reset() -> void {
    std::vector<std::shared_ptr<Track>> released;
    {
        std::lock_guard lock(mutex_);
        released.reserve(tracks_.size());
        for (auto& [_, track] : tracks_)
            released.push_back(std::move(track));
        tracks_.clear();
        current_playing_id_.clear();
    }
    // Do not hold the registry mutex while a source waits for its decode
    // thread. Existing readers retain a shared Track lease and therefore see a
    // valid, atomically-stopped source instead of dangling storage.
    for (const auto& track : released) {
        if (!track || !track->source)
            continue;
        std::lock_guard control_lock(track->control_mutex);
        track->source->Stop();
    }
}

auto MusicPlayer::Unload(const std::string& id) -> bool {
    std::shared_ptr<Track> track;
    {
        std::lock_guard lock(mutex_);
        const auto it = tracks_.find(id);
        if (it == tracks_.end()) {
            LOG_DEBUG(atom::log::audio::Music, "Unload() ignored: unknown track: " + id);
            return false;
        }
        track = std::move(it->second);
        tracks_.erase(it);
        if (current_playing_id_ == id)
            current_playing_id_.clear();
    }
    // Track owns the file before the source, and the local shared lease keeps
    // both alive until Stop() has joined the decode worker.
    if (track && track->source) {
        std::lock_guard control_lock(track->control_mutex);
        track->source->Stop();
    }
    LOG_INFO(atom::log::audio::Music, "Music track unloaded: " + id);
    return true;
}

auto MusicPlayer::SetVolume(const std::string& id, const float volume) -> void {
    const auto track = FindTrack(id);
    if (!track || !track->source)
        return;
    std::lock_guard control_lock(track->control_mutex);
    track->source->SetVolume(atom::numeric::SanitizeVolume(volume));
}

auto MusicPlayer::SetLooping(const std::string& id, const bool loop) -> void {
    const auto track = FindTrack(id);
    if (!track || !track->source)
        return;
    std::lock_guard control_lock(track->control_mutex);
    track->source->SetLooping(loop);
}

auto MusicPlayer::IsLooping(const std::string& id) const -> bool {
    const auto track = FindTrack(id);
    return track && track->source && track->source->IsLooping();
}

auto MusicPlayer::SetMusicVolume(const float volume) -> void {
    mixer_.SetMusicVolume(volume);
    const auto effective = mixer_.GetEffectiveMusicVolume();
    std::vector<std::shared_ptr<Track>> tracks;
    {
        std::lock_guard lock(mutex_);
        tracks.reserve(tracks_.size());
        for (const auto& [_, track] : tracks_)
            tracks.push_back(track);
    }
    for (const auto& track : tracks) {
        if (!track || !track->source)
            continue;
        std::lock_guard control_lock(track->control_mutex);
        track->source->SetVolume(effective);
    }
}

auto MusicPlayer::GetMusicVolume() const -> float {
    return mixer_.GetMusicVolume();
}

auto MusicPlayer::SetNowPlaying(const std::string& id) -> void {
    std::lock_guard lock(mutex_);
    current_playing_id_ = id;
}

auto MusicPlayer::GetNowPlaying() const -> std::string {
    std::lock_guard lock(mutex_);
    RefreshNowPlayingLocked();
    return current_playing_id_;
}

auto MusicPlayer::IsLoaded(const std::string& id) const -> bool {
    std::lock_guard lock(mutex_);
    return tracks_.contains(id);
}

auto MusicPlayer::IsNowPlaying(const std::string& id) const -> bool {
    std::lock_guard lock(mutex_);
    RefreshNowPlayingLocked();
    return current_playing_id_ == id;
}

auto MusicPlayer::IsFinished(const std::string& id) const -> bool {
    const auto track = FindTrack(id);
    if (!track || !track->source || !track->source->IsFinished()) {
        return false;
    }
    std::lock_guard lock(mutex_);
    if (current_playing_id_ == id)
        current_playing_id_.clear();
    return true;
}

auto MusicPlayer::ClearNowPlaying() -> void {
    std::lock_guard lock(mutex_);
    current_playing_id_.clear();
}

auto MusicPlayer::FindTrack(const std::string& id) const -> std::shared_ptr<Track> {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    return it == tracks_.end() ? nullptr : it->second;
}

auto MusicPlayer::RefreshNowPlayingLocked() const -> void {
    if (current_playing_id_.empty()) {
        return;
    }
    const auto it = tracks_.find(current_playing_id_);
    if (it == tracks_.end() || !it->second || !it->second->source ||
        it->second->source->GetState() == atom::audio::AudioSourceState::Stopped) {
        current_playing_id_.clear();
    }
}

auto MusicPlayer::OnAudioBackendChanging() -> void {
    Reset();
}

} // namespace atom

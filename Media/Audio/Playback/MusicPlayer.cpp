#include "MusicPlayer.hpp"

#include <algorithm>

#include <Backend/Contracts/Audio/IAudioBackend.hpp>
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
    std::lock_guard lock(mutex_);
    if (tracks_.contains(id)) {
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
        LOG_ERROR(atom::log::audio::Music, "Failed to decode music: " + label);
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
    // stay alive for as long as the track does. Own it inside the Track.
    Track track{std::move(source), duration_seconds};
    track.file = std::move(streaming->file);
    tracks_.emplace(id, std::move(track));
    LOG_INFO(atom::log::audio::Music, "Music track loaded via VFS: " + id + " (" + label + ")");
    return true;
}

auto MusicPlayer::Load(const std::string& id, const std::string& path) -> bool {
    atom::fs::AssetPath asset_path{};
    if (!atom::fs::AssetPath::TryParse(path, asset_path)) {
        LOG_ERROR(atom::log::audio::Music, "Invalid asset path for music track '" + id + "': " + path);
        return false;
    }
    return Load(id, atom::fs::Vfs::GetInstance(), asset_path);
}

auto MusicPlayer::LoadFromMemory(const std::string& id, const std::string& filename, const void* data,
                                 const std::size_t size) -> bool {
    std::lock_guard lock(mutex_);
    if (tracks_.contains(id)) {
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
    tracks_.emplace(id, Track{std::move(source), duration_seconds});
    LOG_INFO(atom::log::audio::Music,
             "Music track loaded from memory: " + id + " (" + filename + ", " + std::to_string(size) + " bytes)");
    return true;
}

auto MusicPlayer::Play(const std::string& id) -> void {
    Play(id, mixer_.GetEffectiveMusicVolume());
}

auto MusicPlayer::Play(const std::string& id, const float volume) -> void {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source) {
        return;
    }
    if (it->second.source->GetState() == atom::audio::AudioSourceState::Playing) {
        LOG_DEBUG(atom::log::audio::Music, "Play() ignored: track is already playing: " + id);
        return;
    }
    it->second.source->SetVolume(std::clamp(volume, 0.0f, 100.0f));
    it->second.source->Play();
    const auto state = it->second.source->GetState();
    if (state != atom::audio::AudioSourceState::Playing && state != atom::audio::AudioSourceState::Paused) {
        LOG_WARNING(atom::log::audio::Music, "Play() did not start track: " + id);
        return;
    }
    LOG_INFO(atom::log::audio::Music, "Now Playing track: " + id);
    current_playing_id_ = id;
}

auto MusicPlayer::Pause(const std::string& id) -> void {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source) {
        return;
    }
    if (it->second.source->GetState() != atom::audio::AudioSourceState::Playing) {
        LOG_DEBUG(atom::log::audio::Music, "Pause() ignored: track is not playing: " + id);
        return;
    }
    it->second.source->Pause();
    LOG_INFO(atom::log::audio::Music, "Track paused: " + id);
}

auto MusicPlayer::GetState(const std::string& id) const -> atom::audio::AudioSourceState {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source) {
        return atom::audio::AudioSourceState::Stopped;
    }
    return it->second.source->GetState();
}

auto MusicPlayer::Stop(const std::string& id) -> void {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source) {
        return;
    }
    const auto was_active = it->second.source->GetState() != atom::audio::AudioSourceState::Stopped;
    it->second.source->Stop();
    if (was_active) {
        LOG_INFO(atom::log::audio::Music, "Track Stopped: " + id);
    } else {
        LOG_DEBUG(atom::log::audio::Music, "Stop() ignored: track is already stopped: " + id);
    }
    if (current_playing_id_ == id) {
        current_playing_id_.clear();
    }
}

auto MusicPlayer::Seek(const std::string& id, const float seconds) -> bool {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source) {
        LOG_DEBUG(atom::log::audio::Music, "Seek() ignored: unknown track: " + id);
        return false;
    }
    auto target = std::max(seconds, 0.0f);
    if (it->second.duration_seconds > 0.0f)
        target = std::min(target, it->second.duration_seconds);
    if (!it->second.source->SetPlayingOffset(target)) {
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
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source)
        return 0.0f;
    return it->second.source->GetPlayingOffset();
}

auto MusicPlayer::GetDuration(const std::string& id) const -> float {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    return it == tracks_.end() ? 0.0f : it->second.duration_seconds;
}

auto MusicPlayer::IsSeekable(const std::string& id) const -> bool {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    return it != tracks_.end() && it->second.source && it->second.source->IsSeekable();
}

auto MusicPlayer::Reset() -> void {
    std::lock_guard lock(mutex_);
    for (auto& [_, track] : tracks_)
        if (track.source)
            track.source->Stop();
    tracks_.clear();
    current_playing_id_.clear();
}

auto MusicPlayer::SetVolume(const std::string& id, const float volume) -> void {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it != tracks_.end() && it->second.source) {
        it->second.source->SetVolume(std::clamp(volume, 0.0f, 100.0f));
    }
}

auto MusicPlayer::SetLooping(const std::string& id, const bool loop) -> void {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it != tracks_.end() && it->second.source)
        it->second.source->SetLooping(loop);
}

auto MusicPlayer::IsLooping(const std::string& id) const -> bool {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    return it != tracks_.end() && it->second.source && it->second.source->IsLooping();
}

auto MusicPlayer::SetMusicVolume(const float volume) -> void {
    std::lock_guard lock(mutex_);
    mixer_.SetMusicVolume(volume);
    const auto effective = mixer_.GetEffectiveMusicVolume();
    for (auto& [_, track] : tracks_)
        if (track.source)
            track.source->SetVolume(effective);
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
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source || !it->second.source->IsFinished()) {
        return false;
    }
    if (current_playing_id_ == id) {
        current_playing_id_.clear();
    }
    return true;
}

auto MusicPlayer::ClearNowPlaying() -> void {
    std::lock_guard lock(mutex_);
    current_playing_id_.clear();
}

auto MusicPlayer::RefreshNowPlayingLocked() const -> void {
    if (current_playing_id_.empty()) {
        return;
    }
    const auto it = tracks_.find(current_playing_id_);
    if (it == tracks_.end() || !it->second.source ||
        it->second.source->GetState() == atom::audio::AudioSourceState::Stopped) {
        current_playing_id_.clear();
    }
}

auto MusicPlayer::OnAudioBackendChanging() -> void {
    Reset();
}

} // namespace atom

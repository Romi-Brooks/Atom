#include "MusicPlayer.hpp"

#include <algorithm>

#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Backend/Runtime/BackendRuntime.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Resources/AudioClipLoader.hpp>

namespace atom {

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

auto MusicPlayer::Load(const std::string& id, const std::string& file) -> bool {
    std::lock_guard lock(mutex_);
    if (tracks_.contains(id)) {
        LOG_DEBUG(atom::audio::LogChannel::MUSIC, "Music track already loaded, skip: " + id);
        return true;
    }

    AudioClipLoader loader{*decoders_};
    auto streaming = loader.OpenStreaming(file);
    if (!streaming) {
        LOG_ERROR(atom::audio::LogChannel::MUSIC, "Failed to decode music: " + file);
        return false;
    }
    auto& backend = runtime_ ? runtime_->Audio() : *backend_;
    auto source = backend.CreateStreamingMusicSource(std::move(streaming->decoder), streaming->spec);
    if (!source) {
        LOG_ERROR(atom::audio::LogChannel::MUSIC,
                  "Failed to create streaming music source for track '" + id + "': " + file);
        return false;
    }
    tracks_.emplace(id, Track{std::move(source)});
    LOG_INFO(atom::audio::LogChannel::MUSIC, "Music track loaded: " + id + " (" + file + ")");
    return true;
}

auto MusicPlayer::LoadFromMemory(const std::string& id, const std::string& filename, const void* data,
                                 const std::size_t size) -> bool {
    std::lock_guard lock(mutex_);
    if (tracks_.contains(id)) {
        LOG_DEBUG(atom::audio::LogChannel::MUSIC, "Music track already loaded, skip: " + id);
        return true;
    }

    AudioClipLoader loader{*decoders_};
    auto streaming = loader.OpenStreamingFromMemory(filename, data, size);
    if (!streaming) {
        LOG_ERROR(atom::audio::LogChannel::MUSIC, "Failed to decode music from memory: " + filename);
        return false;
    }
    auto& backend = runtime_ ? runtime_->Audio() : *backend_;
    auto source = backend.CreateStreamingMusicSource(std::move(streaming->decoder), streaming->spec);
    if (!source) {
        LOG_ERROR(atom::audio::LogChannel::MUSIC,
                  "Failed to create streaming music source for track '" + id + "': " + filename);
        return false;
    }
    tracks_.emplace(id, Track{std::move(source)});
    LOG_INFO(atom::audio::LogChannel::MUSIC,
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
        LOG_DEBUG(atom::audio::LogChannel::MUSIC, "Play() ignored: track is already playing: " + id);
        return;
    }
    it->second.source->SetVolume(std::clamp(volume, 0.0f, 100.0f));
    it->second.source->Play();
    const auto state = it->second.source->GetState();
    if (state != atom::audio::AudioSourceState::Playing && state != atom::audio::AudioSourceState::Paused) {
        LOG_WARNING(atom::audio::LogChannel::MUSIC, "Play() did not start track: " + id);
        return;
    }
    LOG_INFO(atom::audio::LogChannel::MUSIC, "Now Playing track: " + id);
    current_playing_id_ = id;
}

auto MusicPlayer::Pause(const std::string& id) -> void {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source) {
        return;
    }
    if (it->second.source->GetState() != atom::audio::AudioSourceState::Playing) {
        LOG_DEBUG(atom::audio::LogChannel::MUSIC, "Pause() ignored: track is not playing: " + id);
        return;
    }
    it->second.source->Pause();
    LOG_INFO(atom::audio::LogChannel::MUSIC, "Track paused: " + id);
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
        LOG_INFO(atom::audio::LogChannel::MUSIC, "Track Stopped: " + id);
    } else {
        LOG_DEBUG(atom::audio::LogChannel::MUSIC, "Stop() ignored: track is already stopped: " + id);
    }
    if (current_playing_id_ == id) {
        current_playing_id_.clear();
    }
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

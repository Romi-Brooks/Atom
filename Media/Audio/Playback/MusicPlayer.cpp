#include "MusicPlayer.hpp"

#include <algorithm>

#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Registry/AudioDecoderRegistry.hpp>
#include <Backend/Runtime/BackendRuntime.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Resources/AudioClipLoader.hpp>

namespace atom {

MusicPlayer::MusicPlayer(AudioMixer& mixer)
    : backend_(nullptr),
      decoders_(&BackendRuntime::GetInstance().AudioDecoders()),
      mixer_(mixer), runtime_(&BackendRuntime::GetInstance()) {
    runtime_->AddAudioListener(*this);
}

MusicPlayer::MusicPlayer(
    IAudioBackend& backend, AudioDecoderRegistry& decoders, AudioMixer& mixer)
    : backend_(&backend), decoders_(&decoders), mixer_(mixer) {}

MusicPlayer::~MusicPlayer() {
    if (runtime_) runtime_->RemoveAudioListener(*this);
}

auto MusicPlayer::Load(const std::string& id, const std::string& file) -> bool {
    std::lock_guard lock(mutex_);
    if (tracks_.contains(id)) return true;

    AudioClipLoader loader{*decoders_};
    auto streaming = loader.OpenStreaming(file);
    if (!streaming) {
        LOG_ERROR(LogChannel::ATOM_AUDIO_MUSIC, "Failed to decode music: " + file);
        return false;
    }
    auto& backend = runtime_ ? runtime_->Audio() : *backend_;
    auto source = backend.CreateStreamingMusicSource(
        std::move(streaming->decoder), streaming->spec);
    if (!source) return false;
    tracks_.emplace(id, Track{std::move(source)});
    return true;
}

auto MusicPlayer::Play(const std::string& id) -> void {
    Play(id, mixer_.GetEffectiveMusicVolume());
}

auto MusicPlayer::Play(const std::string& id, const float volume) -> void {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source) return;
    it->second.source->SetVolume(std::clamp(volume, 0.0f, 100.0f));
    it->second.source->Play();
    current_playing_id_ = id;
}

auto MusicPlayer::Stop(const std::string& id) -> void {
    std::lock_guard lock(mutex_);
    const auto it = tracks_.find(id);
    if (it == tracks_.end() || !it->second.source) return;
    it->second.source->Stop();
    if (current_playing_id_ == id) current_playing_id_.clear();
}

auto MusicPlayer::Reset() -> void {
    std::lock_guard lock(mutex_);
    for (auto& [_, track] : tracks_) if (track.source) track.source->Stop();
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
    for (auto& [_, track] : tracks_) if (track.source) track.source->SetVolume(effective);
}

auto MusicPlayer::GetMusicVolume() const -> float { return mixer_.GetMusicVolume(); }
auto MusicPlayer::SetNowPlaying(const std::string& id) -> void { std::lock_guard lock(mutex_); current_playing_id_ = id; }
auto MusicPlayer::GetNowPlaying() const -> std::string { std::lock_guard lock(mutex_); return current_playing_id_; }
auto MusicPlayer::IsLoaded(const std::string& id) const -> bool { std::lock_guard lock(mutex_); return tracks_.contains(id); }
auto MusicPlayer::IsNowPlaying(const std::string& id) const -> bool { std::lock_guard lock(mutex_); return current_playing_id_ == id; }
auto MusicPlayer::ClearNowPlaying() -> void { std::lock_guard lock(mutex_); current_playing_id_.clear(); }

auto MusicPlayer::OnAudioBackendChanging() -> void {
    Reset();
}

auto MusicPlayer::OnAudioDecoderBackendChanging() -> void {
    Reset();
}

} // namespace atom

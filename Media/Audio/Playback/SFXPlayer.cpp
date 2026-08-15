#include "SFXPlayer.hpp"

#include <algorithm>
#include <utility>

#include <Backend/Runtime/BackendRuntime.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>

namespace atom {

SFXPlayer::SFXPlayer(AudioClipCache& clips, AudioMixer& mixer)
    : backend_(nullptr), clips_(clips), mixer_(mixer), runtime_(&BackendRuntime::GetInstance()) {
    runtime_->AddAudioListener(*this);
}

SFXPlayer::SFXPlayer(IAudioBackend& backend, AudioClipCache& clips, AudioMixer& mixer)
    : backend_(&backend), clips_(clips), mixer_(mixer) {}

SFXPlayer::~SFXPlayer() {
    if (runtime_)
        runtime_->RemoveAudioListener(*this);
}

auto SFXPlayer::Load(const std::string& id, const std::string& path) -> bool {
    return clips_.Load(id, path);
}
auto SFXPlayer::GetOrCreatePool(const std::string& id) -> VoicePool* {
    if (const auto it = pools_.find(id); it != pools_.end())
        return it->second.get();
    auto clip = clips_.Get(id);
    if (!clip)
        return nullptr;
    auto& backend = runtime_ ? runtime_->Audio() : *backend_;
    auto pool = std::make_unique<VoicePool>(backend, std::move(clip));
    auto* result = pool.get();
    pools_.emplace(id, std::move(pool));
    return result;
}
auto SFXPlayer::Play(const std::string& id) -> void {
    Play(id, mixer_.GetEffectiveSFXVolume());
}
auto SFXPlayer::Play(const std::string& id, const float volume) -> void {
    auto* pool = GetOrCreatePool(id);
    auto* voice = pool ? pool->Acquire() : nullptr;
    if (!voice)
        return;
    voice->SetVolume(std::clamp(volume, 0.0f, 100.0f));
    voice->Play();
}
auto SFXPlayer::Stop(const std::string& id) -> void {
    if (const auto it = pools_.find(id); it != pools_.end())
        it->second->StopAll();
}
auto SFXPlayer::StopAll() -> void {
    for (auto& [_, pool] : pools_)
        pool->StopAll();
}
auto SFXPlayer::SetVolume(const std::string& id, const float volume) -> void {
    if (const auto it = pools_.find(id); it != pools_.end())
        it->second->SetVolume(std::clamp(volume, 0.0f, 100.0f));
}
auto SFXPlayer::IsLoaded(const std::string& id) const -> bool {
    return clips_.Contains(id);
}
auto SFXPlayer::GetSound(const std::string& id) -> IAudioSource* {
    const auto it = pools_.find(id);
    return it == pools_.end() ? nullptr : it->second->FirstActive();
}
auto SFXPlayer::Unload(const std::string& id) -> bool {
    Stop(id);
    pools_.erase(id);
    return clips_.Unload(id);
}
auto SFXPlayer::Reset() -> void {
    StopAll();
    pools_.clear();
    clips_.Clear();
}
auto SFXPlayer::GetLoadedCount() const -> std::size_t {
    return clips_.Size();
}

auto SFXPlayer::OnAudioBackendChanging() -> void {
    Reset();
}

} // namespace atom

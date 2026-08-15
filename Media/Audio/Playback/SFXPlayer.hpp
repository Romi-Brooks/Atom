#ifndef ATOM_SFX_PLAYER_HPP
#define ATOM_SFX_PLAYER_HPP

#include <memory>
#include <string>
#include <unordered_map>

#include <Backend/Runtime/IAudioBackendChangeListener.hpp>
#include <Media/Audio/Playback/VoicePool.hpp>
#include <Media/Audio/Resources/AudioClipCache.hpp>

namespace atom {
class AudioMixer;
class BackendRuntime;
class IAudioBackend;

class SFXPlayer final : public IAudioBackendChangeListener {
public:
    SFXPlayer(AudioClipCache& clips, AudioMixer& mixer);
    SFXPlayer(IAudioBackend& backend, AudioClipCache& clips, AudioMixer& mixer);
    ~SFXPlayer() override;

    auto Load(const std::string& id, const std::string& path) -> bool;
    auto Play(const std::string& id) -> void;
    auto Play(const std::string& id, float volume) -> void;
    auto Stop(const std::string& id) -> void;
    auto StopAll() -> void;
    auto SetVolume(const std::string& id, float volume) -> void;
    [[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;
    [[nodiscard]] auto GetSound(const std::string& id) -> IAudioSource*;
    auto Unload(const std::string& id) -> bool;
    auto Reset() -> void;
    [[nodiscard]] auto GetLoadedCount() const -> std::size_t;

    auto OnAudioBackendChanging() -> void override;

private:
    auto GetOrCreatePool(const std::string& id) -> VoicePool*;

    IAudioBackend* backend_;
    AudioClipCache& clips_;
    AudioMixer& mixer_;
    BackendRuntime* runtime_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<VoicePool>> pools_;
};

using SFX = SFXPlayer;
} // namespace atom

#endif

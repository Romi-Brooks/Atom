#ifndef ATOM_SFX_PLAYER_HPP
#define ATOM_SFX_PLAYER_HPP

#include <memory>
#include <string>
#include <unordered_map>

#include <Backend/Contracts/Numeric/NumericContract.hpp>
#include <Backend/Runtime/IAudioBackendChangeListener.hpp>
#include <Media/Audio/Playback/VoicePool.hpp>
#include <Media/Audio/Resources/AudioClipCache.hpp>

namespace atom::audio {
class IAudioBackend;
}

namespace atom::backend {
class BackendRuntime;
}

namespace atom {
class AudioMixer;

class SFXPlayer final : public atom::backend::IAudioBackendChangeListener {
    public:
        SFXPlayer(AudioClipCache& clips, AudioMixer& mixer);
        SFXPlayer(atom::audio::IAudioBackend& backend, AudioClipCache& clips, AudioMixer& mixer);
        ~SFXPlayer() override;

        auto Load(const std::string& id, const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path)
            -> bool;

        // Convenience overload: parses `path` as an AssetPath and resolves it
        // through the process-wide default Vfs (atom::fs::Vfs::GetInstance()).
        auto Load(const std::string& id, const std::string& path) -> bool;
        auto Play(const std::string& id) -> void;
        auto Play(const std::string& id, float volume) -> void;
        auto Stop(const std::string& id) -> void;
        auto StopAll() -> void;
        auto SetVolume(const std::string& id, float volume) -> void;
        [[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;
        [[nodiscard]] auto GetSound(const std::string& id) -> atom::audio::IAudioSource*;
        auto Unload(const std::string& id) -> bool;
        auto Reset() -> void;
        [[nodiscard]] auto GetLoadedCount() const -> std::size_t;

        // Voice cap applied to every pool (existing and future). Rejects a value
        // outside [numeric::kMinVoices, numeric::kMaxVoices] (ARCH-006).
        auto SetVoiceLimit(std::size_t voices) -> bool;
        [[nodiscard]] auto GetVoiceLimit() const -> std::size_t {
            return voice_limit_;
        }

        auto OnAudioBackendChanging() -> void override;

    private:
        auto GetOrCreatePool(const std::string& id) -> VoicePool*;

        atom::audio::IAudioBackend* backend_;
        AudioClipCache& clips_;
        AudioMixer& mixer_;
        atom::backend::BackendRuntime* runtime_ = nullptr;
        std::unordered_map<std::string, std::unique_ptr<VoicePool>> pools_;
        std::size_t voice_limit_ = atom::numeric::kDefaultVoices;
};

} // namespace atom

#endif

#ifndef ATOM_BACKEND_RUNTIME_BACKEND_RUNTIME_HPP
#define ATOM_BACKEND_RUNTIME_BACKEND_RUNTIME_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <Backend/Registry/AudioDecoderRegistry.hpp>
#include <Backend/Registry/BackendRegistry.hpp>

namespace atom {

class IAudioBackend;
class IAudioBackendChangeListener;

class BackendRuntime final {
public:
    static auto GetInstance() -> BackendRuntime&;
    ~BackendRuntime();

    BackendRuntime(const BackendRuntime&) = delete;
    auto operator=(const BackendRuntime&) -> BackendRuntime& = delete;

    [[nodiscard]] auto Audio() -> IAudioBackend&;
    [[nodiscard]] auto AudioDecoders() -> AudioDecoderRegistry&;
    [[nodiscard]] auto Registry() -> BackendRegistry&;

    // Global switch for the playback backend: attached players stop and
    // unregister their IDs first. Prefer settings/menu screens with very few
    // registered IDs. See README-CN.md.
    auto SetAudioBackend(std::string_view id) -> bool;
    [[nodiscard]] auto GetAudioBackendId() const -> const std::string&;

    // Register the engine's built-in format decoders (.wav, .mp3, ...) into an
    // AudioDecoderRegistry. Used by the global runtime; also callable for
    // explicitly injected registries (tests / standalone tools).
    static auto RegisterDefaultAudioDecoders(AudioDecoderRegistry& decoders) -> void;

    auto AddAudioListener(IAudioBackendChangeListener& listener) -> void;
    auto RemoveAudioListener(IAudioBackendChangeListener& listener) -> void;

private:
    BackendRuntime();

    auto RegisterAvailableBackends() -> void;
    auto NotifyAudioBackendChanging() -> void;

    BackendRegistry registry_;
    AudioDecoderRegistry audio_decoders_;
    std::unique_ptr<IAudioBackend> audio_backend_;
    std::string audio_backend_id_;
    std::vector<IAudioBackendChangeListener*> audio_listeners_;
};

} // namespace atom

#endif

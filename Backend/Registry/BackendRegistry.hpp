#ifndef ATOM_BACKEND_REGISTRY_BACKEND_REGISTRY_HPP
#define ATOM_BACKEND_REGISTRY_BACKEND_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace atom {

class IAudioBackend;
class AudioDecoderRegistry;

class BackendRegistry final {
public:
    using AudioBackendFactory = std::function<std::unique_ptr<IAudioBackend>()>;
    using AudioDecoderInstaller = std::function<bool(AudioDecoderRegistry&)>;

    // Advanced extension API. Read Backend/Runtime/README-CN.md before
    // registering custom factories or exposing selection to game code.
    auto RegisterAudioBackend(std::string id, AudioBackendFactory factory) -> bool;
    auto RegisterAudioDecoderBackend(std::string id, AudioDecoderInstaller installer) -> bool;

    [[nodiscard]] auto CreateAudioBackend(std::string_view id) const -> std::unique_ptr<IAudioBackend>;
    [[nodiscard]] auto InstallAudioDecoderBackend(std::string_view id, AudioDecoderRegistry& registry) const -> bool;
    [[nodiscard]] auto ContainsAudioBackend(std::string_view id) const -> bool;
    [[nodiscard]] auto ContainsAudioDecoderBackend(std::string_view id) const -> bool;

private:
    static auto NormalizeId(std::string_view id) -> std::string;

    std::unordered_map<std::string, AudioBackendFactory> audio_backends_;
    std::unordered_map<std::string, AudioDecoderInstaller> audio_decoder_backends_;
};

} // namespace atom

#endif

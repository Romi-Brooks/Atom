#include "BackendRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Registry/AudioDecoderRegistry.hpp>

namespace atom {

auto BackendRegistry::NormalizeId(const std::string_view id) -> std::string {
    std::string normalized{id};
    std::ranges::transform(normalized, normalized.begin(), [](const unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return normalized;
}

auto BackendRegistry::RegisterAudioBackend(
    std::string id, AudioBackendFactory factory) -> bool {
    if (!factory) return false;
    return audio_backends_.emplace(NormalizeId(id), std::move(factory)).second;
}

auto BackendRegistry::RegisterAudioDecoderBackend(
    std::string id, AudioDecoderInstaller installer) -> bool {
    if (!installer) return false;
    return audio_decoder_backends_.emplace(NormalizeId(id), std::move(installer)).second;
}

auto BackendRegistry::CreateAudioBackend(const std::string_view id) const
    -> std::unique_ptr<IAudioBackend> {
    const auto it = audio_backends_.find(NormalizeId(id));
    return it == audio_backends_.end() ? nullptr : it->second();
}

auto BackendRegistry::InstallAudioDecoderBackend(
    const std::string_view id, AudioDecoderRegistry& registry) const -> bool {
    const auto it = audio_decoder_backends_.find(NormalizeId(id));
    return it != audio_decoder_backends_.end() && it->second(registry);
}

auto BackendRegistry::ContainsAudioBackend(const std::string_view id) const -> bool {
    return audio_backends_.contains(NormalizeId(id));
}

auto BackendRegistry::ContainsAudioDecoderBackend(const std::string_view id) const -> bool {
    return audio_decoder_backends_.contains(NormalizeId(id));
}

} // namespace atom

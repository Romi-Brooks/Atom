#ifndef ATOM_BACKEND_AUDIO_DECODER_REGISTRY_HPP
#define ATOM_BACKEND_AUDIO_DECODER_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom {

class AudioDecoderRegistry final {
public:
    using Factory = std::function<std::unique_ptr<IAudioDecoder>()>;

    auto Register(std::string extension, Factory factory) -> bool;
    auto Replace(std::string extension, Factory factory) -> bool;
    auto Unregister(std::string_view extension) -> bool;
    // Register a decoder that answers extension queries when the active backend
    // has no factory for it (e.g. ".mp3" always degrades to the minimp3-based
    // decoder regardless of which decoder backend is active).
    auto RegisterFallback(std::string extension, Factory factory) -> bool;
    [[nodiscard]] auto CreateForFile(std::string_view filepath) const -> std::unique_ptr<IAudioDecoder>;
    [[nodiscard]] auto Contains(std::string_view extension) const -> bool;

private:
    static auto NormalizeExtension(std::string_view extension) -> std::string;
    std::unordered_map<std::string, Factory> factories_;
    std::unordered_map<std::string, Factory> fallback_factories_;
};

} // namespace atom

#endif // ATOM_BACKEND_AUDIO_DECODER_REGISTRY_HPP
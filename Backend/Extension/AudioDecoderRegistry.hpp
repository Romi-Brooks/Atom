#ifndef ATOM_BACKEND_AUDIO_DECODER_REGISTRY_HPP
#define ATOM_BACKEND_AUDIO_DECODER_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom::audio {

class AudioDecoderRegistry final {
    public:
        using Factory = std::function<std::unique_ptr<IAudioDecoder>()>;

        auto Register(std::string extension, Factory factory) -> bool;
        auto Replace(std::string extension, Factory factory) -> bool;
        auto Unregister(std::string_view extension) -> bool;
        [[nodiscard]] auto CreateForFile(std::string_view filepath) const -> std::unique_ptr<IAudioDecoder>;
        [[nodiscard]] auto Contains(std::string_view extension) const -> bool;

    private:
        static auto NormalizeExtension(std::string_view extension) -> std::string;
        std::unordered_map<std::string, Factory> factories_;
};

} // namespace atom::audio

#endif // ATOM_BACKEND_AUDIO_DECODER_REGISTRY_HPP

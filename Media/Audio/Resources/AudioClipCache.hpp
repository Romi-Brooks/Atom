#ifndef ATOM_AUDIO_CLIP_CACHE_HPP
#define ATOM_AUDIO_CLIP_CACHE_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Media/Audio/Resources/AudioClipLoader.hpp>

namespace atom {

class AudioClipCache final {
public:
    AudioClipCache();
    explicit AudioClipCache(AudioDecoderRegistry& decoders) : loader_(decoders) {}

    auto Load(const std::string& id, const std::string& path) -> bool;
    [[nodiscard]] auto Get(const std::string& id) const -> std::shared_ptr<const DecodedAudio>;
    [[nodiscard]] auto Contains(const std::string& id) const -> bool;
    auto Unload(const std::string& id) -> bool;
    auto Clear() -> void;
    [[nodiscard]] auto Size() const -> std::size_t;

private:
    AudioClipLoader loader_;
    std::unordered_map<std::string, std::shared_ptr<const DecodedAudio>> clips_;
};

} // namespace atom

#endif // ATOM_AUDIO_CLIP_CACHE_HPP

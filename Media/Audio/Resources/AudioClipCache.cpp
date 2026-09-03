#include "AudioClipCache.hpp"

#include <Backend/Runtime/BackendRuntime.hpp>

namespace atom {

AudioClipCache::AudioClipCache() : loader_(atom::backend::BackendRuntime::GetInstance().AudioDecoders()) {}

auto AudioClipCache::Load(const std::string& id, const std::string& path) -> bool {
    if (clips_.contains(id))
        return true;
    auto clip = loader_.Load(path);
    if (!clip)
        return false;
    clips_.emplace(id, std::make_shared<const atom::audio::DecodedAudio>(std::move(*clip)));
    return true;
}

auto AudioClipCache::Get(const std::string& id) const -> std::shared_ptr<const atom::audio::DecodedAudio> {
    const auto it = clips_.find(id);
    return it == clips_.end() ? nullptr : it->second;
}

auto AudioClipCache::Contains(const std::string& id) const -> bool {
    return clips_.contains(id);
}
auto AudioClipCache::Unload(const std::string& id) -> bool {
    return clips_.erase(id) > 0;
}
auto AudioClipCache::Clear() -> void {
    clips_.clear();
}
auto AudioClipCache::Size() const -> std::size_t {
    return clips_.size();
}

} // namespace atom

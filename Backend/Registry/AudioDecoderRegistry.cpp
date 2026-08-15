#include "AudioDecoderRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <utility>

#include <Log/LogSystem.hpp>

namespace atom {

auto AudioDecoderRegistry::NormalizeExtension(std::string_view extension) -> std::string {
    std::string normalized{extension};
    if (!normalized.empty() && normalized.front() != '.') {
        normalized.insert(normalized.begin(), '.');
    }
    std::ranges::transform(normalized, normalized.begin(),
                           [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return normalized;
}

auto AudioDecoderRegistry::Register(std::string extension, Factory factory) -> bool {
    if (!factory)
        return false;
    auto normalized = NormalizeExtension(extension);
    if (normalized.empty())
        return false;
    return factories_.emplace(std::move(normalized), std::move(factory)).second;
}

auto AudioDecoderRegistry::Replace(std::string extension, Factory factory) -> bool {
    if (!factory)
        return false;
    auto normalized = NormalizeExtension(extension);
    if (normalized.empty())
        return false;
    factories_.insert_or_assign(std::move(normalized), std::move(factory));
    return true;
}

auto AudioDecoderRegistry::Unregister(std::string_view extension) -> bool {
    return factories_.erase(NormalizeExtension(extension)) > 0;
}

auto AudioDecoderRegistry::RegisterFallback(std::string extension, Factory factory) -> bool {
    if (!factory)
        return false;
    auto normalized = NormalizeExtension(extension);
    if (normalized.empty())
        return false;
    return fallback_factories_.emplace(std::move(normalized), std::move(factory)).second;
}

auto AudioDecoderRegistry::CreateForFile(std::string_view filepath) const -> std::unique_ptr<IAudioDecoder> {
    const auto dot = filepath.find_last_of('.');
    if (dot == std::string_view::npos)
        return nullptr;
    const auto extension = NormalizeExtension(filepath.substr(dot));
    const auto it = factories_.find(extension);
    if (it != factories_.end())
        return it->second();

    const auto fallback_it = fallback_factories_.find(extension);
    if (fallback_it == fallback_factories_.end())
        return nullptr;
    LOG_DEBUG(LogChannel::ATOM_AUDIO_MUSIC,
              "No '" + extension + "' decoder in the active backend, falling back to the registered fallback decoder");
    return fallback_it->second();
}

auto AudioDecoderRegistry::Contains(std::string_view extension) const -> bool {
    const auto normalized = NormalizeExtension(extension);
    return factories_.contains(normalized) || fallback_factories_.contains(normalized);
}

} // namespace atom
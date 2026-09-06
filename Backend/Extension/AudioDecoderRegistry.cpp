#include "AudioDecoderRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <utility>

namespace atom::audio {

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

auto AudioDecoderRegistry::CreateForFile(std::string_view filepath) const -> std::unique_ptr<IAudioDecoder> {
    const auto dot = filepath.find_last_of('.');
    if (dot == std::string_view::npos)
        return nullptr;
    const auto it = factories_.find(NormalizeExtension(filepath.substr(dot)));
    return it == factories_.end() ? nullptr : it->second();
}

auto AudioDecoderRegistry::Contains(std::string_view extension) const -> bool {
    return factories_.contains(NormalizeExtension(extension));
}

} // namespace atom::audio

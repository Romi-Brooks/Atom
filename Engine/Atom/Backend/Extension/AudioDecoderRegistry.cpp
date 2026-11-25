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

auto AudioDecoderRegistry::Register(std::string extension, Factory factory, std::string name) -> bool {
    if (!factory)
        return false;
    auto normalized = NormalizeExtension(extension);
    if (normalized.empty())
        return false;
    // First registration wins: the preferred decoder is never silently replaced.
    const auto [it, inserted] = chains_.try_emplace(std::move(normalized), std::vector<Candidate>{});
    if (!inserted)
        return false;
    it->second.push_back(Candidate{std::move(name), std::move(factory)});
    return true;
}

auto AudioDecoderRegistry::RegisterFallback(std::string extension, Factory factory, std::string name) -> bool {
    if (!factory)
        return false;
    auto normalized = NormalizeExtension(extension);
    if (normalized.empty())
        return false;
    chains_[std::move(normalized)].push_back(Candidate{std::move(name), std::move(factory)});
    return true;
}

auto AudioDecoderRegistry::Replace(std::string extension, Factory factory, std::string name) -> bool {
    if (!factory)
        return false;
    auto normalized = NormalizeExtension(extension);
    if (normalized.empty())
        return false;
    chains_.insert_or_assign(std::move(normalized), std::vector<Candidate>{Candidate{std::move(name), std::move(factory)}});
    return true;
}

auto AudioDecoderRegistry::Unregister(std::string_view extension) -> bool {
    return chains_.erase(NormalizeExtension(extension)) > 0;
}

auto AudioDecoderRegistry::CreateForFile(std::string_view filepath) const -> std::unique_ptr<IAudioDecoder> {
    const auto dot = filepath.find_last_of('.');
    if (dot == std::string_view::npos)
        return nullptr;
    const auto it = chains_.find(NormalizeExtension(filepath.substr(dot)));
    if (it == chains_.end() || it->second.empty() || !it->second.front().factory)
        return nullptr;
    return it->second.front().factory();
}

auto AudioDecoderRegistry::CandidatesForFile(std::string_view filepath) const -> std::vector<Candidate> {
    const auto dot = filepath.find_last_of('.');
    if (dot == std::string_view::npos)
        return {};
    const auto it = chains_.find(NormalizeExtension(filepath.substr(dot)));
    return it == chains_.end() ? std::vector<Candidate>{} : it->second;
}

auto AudioDecoderRegistry::Contains(std::string_view extension) const -> bool {
    const auto it = chains_.find(NormalizeExtension(extension));
    return it != chains_.end() && !it->second.empty();
}

} // namespace atom::audio

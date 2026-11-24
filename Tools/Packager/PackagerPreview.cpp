#include "PackagerPreview.hpp"

#include <algorithm>
#include <cctype>

#include <Filesystem/PackageFormat.hpp>
#include <Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Media/Image/ImageBatchDecoder.hpp>
#include <Utilities/Json/JsonReader.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom::packager {
namespace {

namespace kind = fs::package_format::asset_kind;

auto ReadText(fs::PackageFileSystem& package, const fs::AssetPath& path, bool json) -> AssetPreview {
    std::unique_ptr<fs::IFile> file;
    if (package.OpenRead(path, file) != fs::Result::Success)
        return {"Could not open source"};
    constexpr std::uint64_t limit = 128 * 1024;
    const bool truncated = file->Size() > limit;
    std::string text(static_cast<std::size_t>(std::min(file->Size(), limit)), '\0');
    if (file->ReadNext(std::as_writable_bytes(std::span{text})) != fs::Result::Success)
        return {"Could not read source"};
    if (truncated) {
        // The byte cap can land in the middle of a UTF-8 code point.
        for (int trim = 0; trim < 3 && !text.empty() && !IsValidUtf8(text); ++trim)
            text.pop_back();
    }
    if (text.starts_with("\xef\xbb\xbf"))
        text.erase(0, 3);
    if (text.find('\0') != std::string::npos || !IsValidUtf8(text))
        return {"Source is not UTF-8 text"};

    AssetPreview preview{json ? "JSON source" : "Lua source"};
    if (truncated) {
        preview.status += " (first 128 KiB)";
    } else if (json) {
        json::SyntaxError error;
        if (json::TryParse(text, error))
            preview.status = "JSON syntax valid";
        else
            preview.status = "JSON syntax error at " + std::to_string(error.line) + ":" + std::to_string(error.column);
    }
    for (std::size_t start = 0; start < text.size();) {
        auto end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        auto display_end = std::min(start + 512, end);
        while (display_end < end && (static_cast<unsigned char>(text[display_end]) & 0xc0) == 0x80)
            --display_end;
        auto line = text.substr(start, display_end - start);
        if (display_end != end)
            line += "...";
        std::erase(line, '\r');
        std::replace(line.begin(), line.end(), '\t', ' ');
        preview.lines.push_back(std::move(line));
        start = end + 1;
    }
    return preview;
}

} // namespace

auto ReadPreview(const std::string& archive, const fs::PackagedAssetInfo& asset) -> AssetPreview {
    std::string name = asset.path;
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool json = name.ends_with(".json") || name.ends_with(".jsonc");
    const bool lua = name.ends_with(".lua");
    if (!json && !lua && asset.kind != kind::texture && asset.kind != kind::audio)
        return {"Preview unavailable for this type"};

    std::unique_ptr<fs::PackageFileSystem> package;
    const auto path = fs::AssetPath::Parse("preview://" + asset.path);
    if (!path || fs::PackageFileSystem::Create("preview", archive, package) != fs::Result::Success)
        return {"Could not open package resource"};
    if (json || lua)
        return ReadText(*package, *path, json);
    if (asset.kind == kind::texture) {
        if (asset.size > 32 * 1024 * 1024)
            return {"Image exceeds preview size limit"};
        auto decoded = image::ImageBatchDecoder::Decode(*package, *path);
        if (!decoded)
            return {"Image decode failed"};
        return {std::to_string(decoded->width) + " x " + std::to_string(decoded->height) + " px", {},
                std::move(*decoded)};
    }

    AssetPreview preview{"Audio preview"};
    if (auto tags = audio::AudioMetadataReader::Read(*package, *path)) {
        if (!tags->title.empty())
            preview.lines.push_back(tags->title);
        if (!tags->artist.empty())
            preview.lines.push_back(tags->artist);
        if (tags->sampleRate)
            preview.lines.push_back(std::to_string(tags->sampleRate) + " Hz  /  " + std::to_string(tags->channels) +
                                    " ch  /  " + std::to_string(tags->durationSeconds) + " s");
    }
    preview.audio = std::move(package);
    return preview;
}

}

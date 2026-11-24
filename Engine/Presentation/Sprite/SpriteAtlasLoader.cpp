// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Sprite/SpriteAtlasLoader.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Filesystem/FileSystem.hpp>
#include <Log/LogSystem.hpp>

namespace atom::sprite {

namespace {

auto LoadFailure(const std::string_view uri, const std::string& detail) -> Error {
    return MakeError(ErrorCode::InvalidDocument, Module::Core, "SpriteAtlasLoader::Load")
        .WithResource(std::string{uri})
        .WithDetail(detail);
}

} // namespace

auto SpriteAtlasLoader::Load(std::string_view uri) -> Result<const SpriteAtlas*> {
    const std::string key{uri};
    if (const auto found = cache_.find(key); found != cache_.end())
        return found->second.get();

    fs::AssetPath path{};
    if (!fs::AssetPath::TryParse(key, path))
        return std::unexpected<Error>{LoadFailure(key, "the atlas URI is not a valid asset path")};
    return Load(path);
}

auto SpriteAtlasLoader::Load(const fs::AssetPath& path) -> Result<const SpriteAtlas*> {
    const std::string key{path.String()};
    if (const auto found = cache_.find(key); found != cache_.end())
        return found->second.get();

    std::unique_ptr<fs::IFile> file{};
    if (filesystem_->OpenRead(path, file) != fs::Result::Success || !file)
        return std::unexpected<Error>{LoadFailure(key, "the atlas file could not be opened")};

    std::vector<std::byte> bytes{};
    if (fs::ReadAll(*file, bytes) != fs::Result::Success)
        return std::unexpected<Error>{LoadFailure(key, "the atlas file could not be read")};

    const std::string_view text{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    auto parsed = ParseAtlas(text, key);
    if (!parsed.Ok()) {
        // Log every diagnostic, so a four-error atlas does not hide three of them; the
        // Error carries the first, which is the one a UI shows.
        for (const auto& diagnostic : parsed.diagnostics)
            LOG_WARNING(atom::log::core::Main, "atlas load: " + diagnostic.Format());
        std::string detail = parsed.diagnostics.front().Format();
        if (parsed.diagnostics.size() > 1)
            detail += " (+" + std::to_string(parsed.diagnostics.size() - 1) + " more)";
        return std::unexpected<Error>{LoadFailure(key, detail)};
    }

    auto stored = cache_.emplace(key, std::make_unique<SpriteAtlas>(std::move(*parsed.atlas)));
    return stored.first->second.get();
}

} // namespace atom::sprite

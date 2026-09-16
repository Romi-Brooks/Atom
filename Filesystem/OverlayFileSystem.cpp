/**
 * @file           : OverlayFileSystem.cpp
 * @brief          : Priority overlay filesystem backend implementation.
**/

#include "OverlayFileSystem.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace atom::fs {

OverlayFileSystem::OverlayFileSystem(std::vector<std::shared_ptr<IFileSystem>> backends)
    : backends_(std::move(backends)) {}

OverlayFileSystem::~OverlayFileSystem() = default;

auto OverlayFileSystem::Create(std::vector<std::shared_ptr<IFileSystem>> backends_highest_first,
                               std::unique_ptr<OverlayFileSystem>& output) -> Result {
    output.reset();
    if (backends_highest_first.empty())
        return Result::InvalidPath;
    for (const auto& backend : backends_highest_first) {
        if (!backend)
            return Result::InvalidPath;
    }
    output = std::unique_ptr<OverlayFileSystem>{new OverlayFileSystem{std::move(backends_highest_first)}};
    return Result::Success;
}

auto OverlayFileSystem::Stat(const AssetPath& path, FileInfo& output) const -> Result {
    Result last = Result::NotFound;
    for (const auto& backend : backends_) {
        FileInfo info{};
        const Result result = backend->Stat(path, info);
        if (result == Result::Success) {
            output = info;
            return Result::Success;
        }
        if (result != Result::NotFound && result != Result::InvalidPath)
            last = result;
    }
    return last;
}

auto OverlayFileSystem::OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result {
    output.reset();
    Result last = Result::NotFound;
    for (const auto& backend : backends_) {
        std::unique_ptr<IFile> file{};
        const Result result = backend->OpenRead(path, file);
        if (result == Result::Success) {
            output = std::move(file);
            return Result::Success;
        }
        if (result != Result::NotFound && result != Result::InvalidPath)
            last = result;
    }
    return last;
}

auto OverlayFileSystem::List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result {
    output.clear();

    // Walk lowest priority first so higher-priority entries overwrite names.
    std::map<std::string, DirectoryEntry> merged{};
    bool listed = false;
    Result first_hard_error = Result::Success;
    for (auto it = backends_.rbegin(); it != backends_.rend(); ++it) {
        std::vector<DirectoryEntry> entries{};
        const Result result = (*it)->List(directory, entries);
        if (result == Result::NotFound || result == Result::NotDirectory)
            continue;
        if (result != Result::Success) {
            if (first_hard_error == Result::Success)
                first_hard_error = result;
            continue;
        }
        listed = true;
        for (auto& entry : entries)
            merged.insert_or_assign(entry.name, std::move(entry));
    }

    if (!listed)
        return first_hard_error == Result::Success ? Result::NotFound : first_hard_error;

    output.reserve(merged.size());
    for (auto& [name, entry] : merged)
        output.push_back(std::move(entry));
    std::ranges::sort(output, {}, &DirectoryEntry::name);
    return Result::Success;
}

} // namespace atom::fs

/**
 * @file           : Vfs.cpp
 * @brief          : Named VFS mount table implementation.
**/

#include "Vfs.hpp"

#include <algorithm>
#include <utility>

#include "OverlayFileSystem.hpp"

namespace atom::fs {
namespace {

[[nodiscard]] auto IsValidMountName(const std::string_view mount) -> bool {
    AssetPath root{};
    return !mount.empty() && AssetPath::TryParse(std::string{mount} + "://", root) && root.Mount() == mount;
}

} // namespace

Vfs::~Vfs() = default;

auto Vfs::Mount(const std::string_view mount, const int priority, std::shared_ptr<IFileSystem> backend) -> Result {
    if (!backend || !IsValidMountName(mount))
        return Result::InvalidPath;

    const auto it = std::ranges::find_if(mounts_, [&](const auto& entry) { return entry.first == mount; });
    if (it == mounts_.end()) {
        mounts_.emplace_back(std::string{mount}, std::vector<MountEntry>{{priority, std::move(backend)}});
        std::ranges::sort(mounts_, {}, [](const auto& entry) { return entry.first; });
        return Result::Success;
    }

    auto& entries = it->second;
    entries.push_back(MountEntry{priority, std::move(backend)});
    std::ranges::stable_sort(
        entries, [](const MountEntry& left, const MountEntry& right) { return left.priority > right.priority; });
    return Result::Success;
}

auto Vfs::Unmount(const std::string_view mount, const IFileSystem* backend) -> Result {
    if (!backend)
        return Result::InvalidPath;
    const auto it = std::ranges::find_if(mounts_, [&](const auto& entry) { return entry.first == mount; });
    if (it == mounts_.end())
        return Result::NotFound;

    auto& entries = it->second;
    const auto entry =
        std::ranges::find_if(entries, [&](const MountEntry& candidate) { return candidate.backend.get() == backend; });
    if (entry == entries.end())
        return Result::NotFound;
    entries.erase(entry);
    if (entries.empty())
        mounts_.erase(it);
    return Result::Success;
}

auto Vfs::UnmountAll(const std::string_view mount) -> Result {
    const auto it = std::ranges::find_if(mounts_, [&](const auto& entry) { return entry.first == mount; });
    if (it == mounts_.end())
        return Result::NotFound;
    mounts_.erase(it);
    return Result::Success;
}

auto Vfs::MountCount(const std::string_view mount) const -> std::size_t {
    const auto it = std::ranges::find_if(mounts_, [&](const auto& entry) { return entry.first == mount; });
    return it == mounts_.end() ? 0 : it->second.size();
}

auto Vfs::BackendsFor(const std::string_view mount) const -> std::vector<std::shared_ptr<IFileSystem>> {
    const auto it = std::ranges::find_if(mounts_, [&](const auto& entry) { return entry.first == mount; });
    if (it == mounts_.end())
        return {};

    std::vector<std::shared_ptr<IFileSystem>> backends{};
    backends.reserve(it->second.size());
    for (const auto& entry : it->second)
        backends.push_back(entry.backend);
    return backends;
}

auto Vfs::Stat(const AssetPath& path, FileInfo& output) const -> Result {
    if (!path.IsValid())
        return Result::InvalidPath;
    const auto backends = BackendsFor(path.Mount());
    if (backends.empty())
        return Result::NotFound;

    Result last = Result::NotFound;
    for (const auto& backend : backends) {
        FileInfo info{};
        const Result result = backend->Stat(path, info);
        if (result == Result::Success) {
            output = info;
            return Result::Success;
        }
        if (result == Result::OutsideRoot)
            return result;
        if (result != Result::NotFound && result != Result::InvalidPath)
            last = result;
    }
    return last;
}

auto Vfs::OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result {
    output.reset();
    if (!path.IsValid())
        return Result::InvalidPath;
    const auto backends = BackendsFor(path.Mount());
    if (backends.empty())
        return Result::NotFound;

    Result last = Result::NotFound;
    for (const auto& backend : backends) {
        std::unique_ptr<IFile> file{};
        const Result result = backend->OpenRead(path, file);
        if (result == Result::Success) {
            output = std::move(file);
            return Result::Success;
        }
        if (result == Result::OutsideRoot)
            return result;
        if (result != Result::NotFound && result != Result::InvalidPath)
            last = result;
    }
    return last;
}

auto Vfs::List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result {
    output.clear();
    if (!directory.IsValid())
        return Result::InvalidPath;
    const auto backends = BackendsFor(directory.Mount());
    if (backends.empty())
        return Result::NotFound;

    std::unique_ptr<OverlayFileSystem> overlay{};
    if (OverlayFileSystem::Create(backends, overlay) != Result::Success || !overlay)
        return Result::IoError;
    return overlay->List(directory, output);
}

} // namespace atom::fs

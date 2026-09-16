/**
 * @file           : MemoryFileSystem.cpp
 * @brief          : In-memory filesystem backend implementation.
**/

#include "MemoryFileSystem.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace atom::fs {
namespace {

[[nodiscard]] auto NormalizeRelative(std::string_view relative) -> std::string {
    std::string normalized{relative};
    std::ranges::replace(normalized, '\\', '/');
    while (normalized.starts_with('/'))
        normalized.erase(normalized.begin());
    while (normalized.ends_with('/'))
        normalized.pop_back();
    return normalized;
}

[[nodiscard]] auto ParentOf(const std::string& relative) -> std::string {
    const std::size_t slash = relative.find_last_of('/');
    return slash == std::string::npos ? std::string{} : relative.substr(0, slash);
}

class MemoryNode final {
    public:
        explicit MemoryNode(const EntryType type) : type_(type) {}

        [[nodiscard]] auto Type() const -> EntryType {
            return type_;
        }
        [[nodiscard]] auto Data() const -> const std::shared_ptr<std::vector<std::byte>>& {
            return data_;
        }

        auto SetData(std::span<const std::byte> bytes) -> void {
            type_ = EntryType::File;
            data_ = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
        }

    private:
        EntryType type_{};
        std::shared_ptr<std::vector<std::byte>> data_{};
};

class MemoryReadFile final : public IFile {
    public:
        explicit MemoryReadFile(std::shared_ptr<std::vector<std::byte>> data) : data_(std::move(data)) {}

        [[nodiscard]] auto Size() const -> uint64_t override {
            return data_->size();
        }
        [[nodiscard]] auto Tell() const -> uint64_t override {
            return position_;
        }

        auto Seek(const uint64_t offset) -> Result override {
            if (offset > data_->size())
                return Result::OutOfRange;
            position_ = offset;
            return Result::Success;
        }

        auto ReadAt(const uint64_t offset, const std::span<std::byte> destination) -> Result override {
            if (offset > data_->size() || destination.size() > data_->size() - offset)
                return Result::OutOfRange;
            if (destination.empty())
                return Result::Success;
            std::copy_n(data_->data() + offset, destination.size(), destination.data());
            return Result::Success;
        }

        auto ReadNext(std::span<std::byte> destination) -> Result override {
            const Result result = ReadAt(position_, destination);
            if (result == Result::Success)
                position_ += destination.size();
            return result;
        }

    private:
        std::shared_ptr<std::vector<std::byte>> data_{};
        uint64_t position_ = 0;
};

} // namespace

class MemoryFileSystem::Impl final {
    public:
        explicit Impl(std::string mount) : mount_(std::move(mount)) {}

        [[nodiscard]] auto Resolve(const AssetPath& path) const -> Result {
            if (!path.IsValid())
                return Result::InvalidPath;
            if (path.Mount() != mount_)
                return Result::NotFound;
            return Result::Success;
        }

        auto EnsureAncestors(const std::string& relative) -> Result {
            if (relative.empty())
                return Result::Success;
            std::string current{};
            std::size_t start = 0;
            while (start < relative.size()) {
                const std::size_t slash = relative.find('/', start);
                const std::size_t end = slash == std::string::npos ? relative.size() : slash;
                if (!current.empty())
                    current.push_back('/');
                current.append(relative, start, end - start);
                const auto it = nodes_.find(current);
                if (it == nodes_.end()) {
                    nodes_.try_emplace(current, EntryType::Directory);
                } else if (it->second.Type() != EntryType::Directory) {
                    return Result::NotDirectory;
                }
                if (slash == std::string::npos)
                    break;
                start = slash + 1;
            }
            return Result::Success;
        }

        auto Find(const std::string& relative) const -> const MemoryNode* {
            const auto it = nodes_.find(relative);
            return it == nodes_.end() ? nullptr : &it->second;
        }

        std::string mount_{};
        std::map<std::string, MemoryNode, std::less<>> nodes_{};
};

MemoryFileSystem::MemoryFileSystem(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
MemoryFileSystem::~MemoryFileSystem() = default;

auto MemoryFileSystem::Create(const std::string_view mount, std::unique_ptr<MemoryFileSystem>& output) -> Result {
    output.reset();
    AssetPath mountRoot{};
    if (!AssetPath::TryParse(std::string{mount} + "://", mountRoot) || mountRoot.Mount() != mount)
        return Result::InvalidPath;
    output = std::unique_ptr<MemoryFileSystem>{new MemoryFileSystem{std::make_unique<Impl>(std::string{mount})}};
    return Result::Success;
}

auto MemoryFileSystem::WriteFile(const AssetPath& path, const std::span<const std::byte> data) -> Result {
    const Result resolved = impl_->Resolve(path);
    if (resolved != Result::Success)
        return resolved;
    if (path.IsRoot())
        return Result::NotFile;

    const std::string relative = NormalizeRelative(path.Relative());
    if (relative.empty())
        return Result::InvalidPath;

    const MemoryNode* existing = impl_->Find(relative);
    if (existing && existing->Type() == EntryType::Directory)
        return Result::NotFile;

    const Result ancestors = impl_->EnsureAncestors(ParentOf(relative));
    if (ancestors != Result::Success)
        return ancestors;
    impl_->nodes_.insert_or_assign(relative, MemoryNode{EntryType::File}).first->second.SetData(data);
    return Result::Success;
}

auto MemoryFileSystem::CreateDirectory(const AssetPath& path) -> Result {
    const Result resolved = impl_->Resolve(path);
    if (resolved != Result::Success)
        return resolved;
    if (path.IsRoot())
        return Result::Success;

    const std::string relative = NormalizeRelative(path.Relative());
    if (relative.empty())
        return Result::InvalidPath;
    const MemoryNode* existing = impl_->Find(relative);
    if (existing && existing->Type() == EntryType::File)
        return Result::NotDirectory;
    return impl_->EnsureAncestors(relative);
}

auto MemoryFileSystem::Remove(const AssetPath& path) -> Result {
    const Result resolved = impl_->Resolve(path);
    if (resolved != Result::Success)
        return resolved;
    if (path.IsRoot())
        return Result::NotFile;

    const std::string relative = NormalizeRelative(path.Relative());
    const auto it = impl_->nodes_.find(relative);
    if (it == impl_->nodes_.end())
        return Result::NotFound;
    impl_->nodes_.erase(it);
    return Result::Success;
}

auto MemoryFileSystem::Stat(const AssetPath& path, FileInfo& output) const -> Result {
    const Result resolved = impl_->Resolve(path);
    if (resolved != Result::Success)
        return resolved;

    if (path.IsRoot()) {
        output = {EntryType::Directory, 0};
        return Result::Success;
    }

    const std::string relative = NormalizeRelative(path.Relative());
    const MemoryNode* node = impl_->Find(relative);
    if (!node)
        return Result::NotFound;
    if (node->Type() == EntryType::Directory) {
        output = {EntryType::Directory, 0};
        return Result::Success;
    }
    output = {EntryType::File, node->Data() ? node->Data()->size() : 0};
    return Result::Success;
}

auto MemoryFileSystem::OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result {
    output.reset();
    FileInfo info{};
    const Result status = Stat(path, info);
    if (status != Result::Success)
        return status;
    if (info.type != EntryType::File)
        return Result::NotFile;

    const std::string relative = NormalizeRelative(path.Relative());
    const MemoryNode* node = impl_->Find(relative);
    if (!node || !node->Data())
        return Result::IoError;
    output = std::make_unique<MemoryReadFile>(node->Data());
    return Result::Success;
}

auto MemoryFileSystem::List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result {
    output.clear();
    const Result resolved = impl_->Resolve(directory);
    if (resolved != Result::Success)
        return resolved;

    FileInfo info{};
    const Result status = Stat(directory, info);
    if (status != Result::Success)
        return status;
    if (info.type != EntryType::Directory)
        return Result::NotDirectory;

    const std::string prefix = directory.IsRoot() ? std::string{} : NormalizeRelative(directory.Relative());
    const std::string filter = prefix.empty() ? std::string{} : prefix + "/";

    std::map<std::string, DirectoryEntry> merged{};
    for (const auto& [path, node] : impl_->nodes_) {
        if (!filter.empty()) {
            if (!path.starts_with(filter))
                continue;
            const std::string remainder = path.substr(filter.size());
            if (remainder.empty())
                continue;
            const std::size_t slash = remainder.find('/');
            const std::string name = remainder.substr(0, slash == std::string::npos ? remainder.size() : slash);
            const EntryType type = slash == std::string::npos ? node.Type() : EntryType::Directory;
            auto [it, inserted] = merged.try_emplace(name, DirectoryEntry{name, {type, 0}});
            if (!inserted)
                it->second.info.type = type;
            if (type == EntryType::File && node.Data())
                it->second.info.size = node.Data()->size();
            continue;
        }

        const std::size_t slash = path.find('/');
        const std::string name = path.substr(0, slash == std::string::npos ? path.size() : slash);
        const EntryType type = slash == std::string::npos ? node.Type() : EntryType::Directory;
        auto [it, inserted] = merged.try_emplace(name, DirectoryEntry{name, {type, 0}});
        if (!inserted)
            it->second.info.type = type;
        if (type == EntryType::File && node.Data())
            it->second.info.size = node.Data()->size();
    }

    output.reserve(merged.size());
    for (auto& [name, entry] : merged)
        output.push_back(std::move(entry));
    std::ranges::sort(output, {}, &DirectoryEntry::name);
    return Result::Success;
}

} // namespace atom::fs

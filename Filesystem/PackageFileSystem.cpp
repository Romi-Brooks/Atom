/**
 * @file           : PackageFileSystem.cpp
 * @brief          : APKG v1 package backend implementation.
**/

#include "PackageFileSystem.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <filesystem>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom::fs {
namespace {

namespace native_fs = std::filesystem;

constexpr char kMagic[4] = {'A', 'P', 'K', 'G'};
constexpr uint16_t kVersion = 1;
constexpr uint64_t kMinHeaderSize = 10;
constexpr uint32_t kMaxFileCount = 1'000'000;
constexpr uint16_t kMaxNameLength = 4096;

struct PackageEntry {
        std::string relative{};
        uint64_t offset = 0;
        uint64_t size = 0;
};

[[nodiscard]] auto NormalizeRelative(std::string_view relative) -> std::string {
    std::string normalized{relative};
    std::ranges::replace(normalized, '\\', '/');
    while (!normalized.empty() && normalized.front() == '/')
        normalized.erase(normalized.begin());
    while (!normalized.empty() && normalized.back() == '/')
        normalized.pop_back();
    return normalized;
}

[[nodiscard]] auto IsSafePackageRelative(const std::string& relative) -> bool {
    if (relative.empty())
        return false;
    if (relative.find('\0') != std::string::npos)
        return false;
    if (relative.starts_with('/') || relative.contains("//"))
        return false;

    const native_fs::path path = atom::PathFromUtf8(relative);
    if (path.is_absolute() || path.has_root_name())
        return false;
    for (const auto& part : path) {
        if (part == ".." || part == ".")
            return false;
    }
    return true;
}

class PackageReadFile final : public IFile {
    public:
        PackageReadFile(std::ifstream stream, const uint64_t base, const uint64_t size)
            : stream_(std::move(stream)), base_(base), size_(size) {}

        [[nodiscard]] auto IsOpen() const -> bool {
            return stream_.is_open();
        }
        [[nodiscard]] auto Size() const -> uint64_t override {
            return size_;
        }
        [[nodiscard]] auto Tell() const -> uint64_t override {
            return position_;
        }

        auto Seek(const uint64_t offset) -> Result override {
            if (offset > size_)
                return Result::OutOfRange;
            position_ = offset;
            return Result::Success;
        }

        auto ReadAt(const uint64_t offset, const std::span<std::byte> destination) -> Result override {
            if (!RangeFits(offset, destination.size()))
                return Result::OutOfRange;
            if (destination.empty())
                return Result::Success;

            stream_.clear();
            stream_.seekg(static_cast<std::streamoff>(base_ + offset), std::ios::beg);
            if (!stream_)
                return Result::IoError;
            stream_.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(destination.size()));
            return stream_ ? Result::Success : Result::IoError;
        }

        auto ReadNext(std::span<std::byte> destination) -> Result override {
            if (!RangeFits(position_, destination.size()))
                return Result::OutOfRange;
            if (destination.empty())
                return Result::Success;

            stream_.clear();
            stream_.seekg(static_cast<std::streamoff>(base_ + position_), std::ios::beg);
            if (!stream_)
                return Result::IoError;
            stream_.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(destination.size()));
            if (!stream_)
                return Result::IoError;
            position_ += destination.size();
            return Result::Success;
        }

    private:
        [[nodiscard]] auto RangeFits(const uint64_t offset, const std::size_t length) const -> bool {
            return offset <= size_ && length <= size_ - offset &&
                   offset <= static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max()) &&
                   length <= static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
        }

        std::ifstream stream_{};
        uint64_t base_ = 0;
        uint64_t size_ = 0;
        uint64_t position_ = 0;
};

} // namespace

class PackageFileSystem::Impl final {
    public:
        Impl(std::string mount, std::string package_path)
            : mount_(std::move(mount)), package_path_(std::move(package_path)) {}

        [[nodiscard]] auto Resolve(const AssetPath& path) const -> Result {
            if (!path.IsValid())
                return Result::InvalidPath;
            if (path.Mount() != mount_)
                return Result::NotFound;
            return Result::Success;
        }

        [[nodiscard]] auto FindFile(const std::string& relative) const -> const PackageEntry* {
            const auto it = index_.find(relative);
            return it == index_.end() ? nullptr : &entries_[it->second];
        }

        auto OpenEntry(const PackageEntry& entry) const -> std::unique_ptr<PackageReadFile> {
            std::ifstream stream{atom::PathFromUtf8(package_path_), std::ios::binary};
            if (!stream.is_open())
                return nullptr;
            auto file = std::make_unique<PackageReadFile>(std::move(stream), entry.offset, entry.size);
            return file->IsOpen() ? std::move(file) : nullptr;
        }

        std::string mount_{};
        std::string package_path_{};
        std::vector<PackageEntry> entries_{};
        std::unordered_map<std::string, std::size_t> index_{};
};

PackageFileSystem::PackageFileSystem(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
PackageFileSystem::~PackageFileSystem() = default;

auto PackageFileSystem::Create(const std::string_view mount, const std::string_view package_utf8,
                               std::unique_ptr<PackageFileSystem>& output) -> Result {
    output.reset();
    if (package_utf8.empty() || !atom::IsValidUtf8(std::string{package_utf8}))
        return Result::InvalidPath;

    AssetPath mountRoot{};
    if (!AssetPath::TryParse(std::string{mount} + "://", mountRoot) || mountRoot.Mount() != mount)
        return Result::InvalidPath;

    const native_fs::path package_path = atom::PathFromUtf8(std::string{package_utf8});
    std::error_code error;
    if (!native_fs::exists(package_path, error) || error)
        return Result::NotFound;
    if (!native_fs::is_regular_file(package_path, error) || error)
        return Result::NotFile;
    const uintmax_t package_size = native_fs::file_size(package_path, error);
    if (error || package_size < kMinHeaderSize + sizeof(uint64_t))
        return Result::IoError;

    std::ifstream input{package_path, std::ios::binary};
    if (!input.is_open())
        return Result::IoError;

    char magic[4]{};
    input.read(magic, 4);
    if (!input || !std::equal(std::begin(kMagic), std::end(kMagic), magic))
        return Result::IoError;

    uint16_t version = 0;
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!input || version != kVersion)
        return Result::IoError;

    uint32_t file_count = 0;
    input.read(reinterpret_cast<char*>(&file_count), sizeof(file_count));
    if (!input || file_count == 0 || file_count > kMaxFileCount)
        return Result::IoError;

    input.seekg(-static_cast<std::streamoff>(sizeof(uint64_t)), std::ios::end);
    if (!input)
        return Result::IoError;
    uint64_t table_offset = 0;
    input.read(reinterpret_cast<char*>(&table_offset), sizeof(table_offset));
    if (!input || table_offset < kMinHeaderSize || table_offset >= package_size)
        return Result::IoError;

    input.seekg(static_cast<std::streamoff>(table_offset));
    if (!input)
        return Result::IoError;

    auto impl = std::make_unique<Impl>(std::string{mount}, atom::PathToUtf8(package_path));
    impl->entries_.reserve(file_count);
    for (uint32_t index = 0; index < file_count; ++index) {
        uint16_t name_length = 0;
        input.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));
        if (!input || name_length == 0 || name_length > kMaxNameLength)
            return Result::IoError;

        std::string name(name_length, '\0');
        input.read(name.data(), name_length);
        if (!input || !atom::IsValidUtf8(name))
            return Result::IoError;
        name = NormalizeRelative(name);
        if (!IsSafePackageRelative(name))
            return Result::IoError;

        uint64_t offset = 0;
        uint64_t size = 0;
        input.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        input.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!input || offset < kMinHeaderSize || offset > table_offset || size > table_offset - offset)
            return Result::IoError;

        uint8_t type_length = 0;
        input.read(reinterpret_cast<char*>(&type_length), sizeof(type_length));
        if (!input)
            return Result::IoError;
        if (type_length > 0) {
            std::string type(type_length, '\0');
            input.read(type.data(), type_length);
            if (!input)
                return Result::IoError;
        }

        if (impl->index_.contains(name))
            return Result::IoError;
        impl->index_[name] = impl->entries_.size();
        impl->entries_.push_back(PackageEntry{name, offset, size});
    }

    output = std::unique_ptr<PackageFileSystem>{new PackageFileSystem{std::move(impl)}};
    return Result::Success;
}

auto PackageFileSystem::Stat(const AssetPath& path, FileInfo& output) const -> Result {
    const Result resolved = impl_->Resolve(path);
    if (resolved != Result::Success)
        return resolved;

    if (path.IsRoot()) {
        output = {EntryType::Directory, 0};
        return Result::Success;
    }

    const std::string relative = NormalizeRelative(path.Relative());
    if (const PackageEntry* file = impl_->FindFile(relative)) {
        output = {EntryType::File, file->size};
        return Result::Success;
    }

    const std::string prefix = relative + "/";
    for (const PackageEntry& entry : impl_->entries_) {
        if (entry.relative.starts_with(prefix)) {
            output = {EntryType::Directory, 0};
            return Result::Success;
        }
    }
    return Result::NotFound;
}

auto PackageFileSystem::OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result {
    output.reset();
    FileInfo info{};
    const Result status = Stat(path, info);
    if (status != Result::Success)
        return status;
    if (info.type != EntryType::File)
        return Result::NotFile;

    const std::string relative = NormalizeRelative(path.Relative());
    const PackageEntry* entry = impl_->FindFile(relative);
    if (!entry)
        return Result::NotFound;
    auto file = impl_->OpenEntry(*entry);
    if (!file)
        return Result::IoError;
    output = std::move(file);
    return Result::Success;
}

auto PackageFileSystem::List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result {
    output.clear();
    FileInfo info{};
    const Result status = Stat(directory, info);
    if (status != Result::Success)
        return status;
    if (info.type != EntryType::Directory)
        return Result::NotDirectory;

    const std::string prefix = directory.IsRoot() ? std::string{} : NormalizeRelative(directory.Relative()) + "/";
    std::map<std::string, DirectoryEntry> merged{};
    for (const PackageEntry& entry : impl_->entries_) {
        if (!prefix.empty()) {
            if (!entry.relative.starts_with(prefix))
                continue;
            const std::string remainder = entry.relative.substr(prefix.size());
            if (remainder.empty())
                continue;
            const std::size_t slash = remainder.find('/');
            const std::string name = remainder.substr(0, slash == std::string::npos ? remainder.size() : slash);
            if (slash == std::string::npos)
                merged.insert_or_assign(name, DirectoryEntry{name, {EntryType::File, entry.size}});
            else
                merged.try_emplace(name, DirectoryEntry{name, {EntryType::Directory, 0}});
            continue;
        }

        const std::size_t slash = entry.relative.find('/');
        const std::string name = entry.relative.substr(0, slash == std::string::npos ? entry.relative.size() : slash);
        if (slash == std::string::npos)
            merged.insert_or_assign(name, DirectoryEntry{name, {EntryType::File, entry.size}});
        else
            merged.try_emplace(name, DirectoryEntry{name, {EntryType::Directory, 0}});
    }

    output.reserve(merged.size());
    for (auto& [name, entry] : merged)
        output.push_back(std::move(entry));
    std::ranges::sort(output, {}, &DirectoryEntry::name);
    return Result::Success;
}

} // namespace atom::fs

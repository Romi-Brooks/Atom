/**
 * @file           : FileSystem.cpp
 * @brief          : Native filesystem backend implementation.
**/

#include "FileSystem.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

#include <Utilities/Utf8/Utf8.hpp>

namespace atom::fs {
namespace {

namespace native_fs = std::filesystem;

[[nodiscard]] auto IsWithinRoot(const native_fs::path& root, const native_fs::path& candidate) -> bool {
    // Different root names (e.g. C: vs D:, or a drive vs a UNC share) can never
    // be inside one another. Reject them up front so `relative` never has to
    // manufacture a path across volume boundaries (which some implementations
    // resolve to a surprising non-empty value rather than reporting an error).
    if (root.has_root_name() != candidate.has_root_name())
        return false;
    if (root.has_root_name() && candidate.has_root_name() && root.root_name() != candidate.root_name())
        return false;

    std::error_code error;
    const native_fs::path relative = native_fs::relative(candidate, root, error);
    if (error)
        return false;
    if (relative.empty())
        return candidate == root;
    for (const auto& part : relative) {
        if (part == "..")
            return false;
    }
    return !relative.is_absolute();
}

} // namespace

class NativeFileSystem::Impl final {
    public:
        Impl(std::string mount, native_fs::path root) : mount_(std::move(mount)), root_(std::move(root)) {}

        [[nodiscard]] auto Resolve(const AssetPath& path, native_fs::path& output) const -> Result {
            if (!path.IsValid())
                return Result::InvalidPath;
            if (path.Mount() != mount_)
                return Result::NotFound;
            native_fs::path candidate = root_;
            if (!path.IsRoot())
                candidate /= atom::PathFromUtf8(std::string{path.Relative()});

            std::error_code error;
            const native_fs::path canonical = native_fs::weakly_canonical(candidate, error);
            if (error)
                return error == std::errc::no_such_file_or_directory ? Result::NotFound : Result::IoError;
            if (!IsWithinRoot(root_, canonical))
                return Result::OutsideRoot;
            output = canonical;
            return Result::Success;
        }

        std::string mount_{};
        native_fs::path root_{};
};

class NativeReadFile final : public IFile {
    public:
        NativeReadFile(const native_fs::path& path, const uint64_t size)
            : stream_(path, std::ios::binary), size_(size) {}

        [[nodiscard]] auto IsOpen() const -> bool {
            return stream_.is_open();
        }
        [[nodiscard]] auto Size() const -> uint64_t override {
            return size_;
        }
        [[nodiscard]] auto Tell() const -> uint64_t override {
            return position_;
        }

        auto ReadAt(const uint64_t offset, const std::span<std::byte> destination) -> Result override {
            if (!RangeFits(offset, destination.size()))
                return Result::OutOfRange;
            if (destination.empty())
                return Result::Success;

            stream_.clear();
            stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            if (!stream_)
                return Result::IoError;
            stream_.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(destination.size()));
            return stream_ ? Result::Success : Result::IoError;
        }

        auto Seek(const uint64_t offset) -> Result override {
            if (offset > size_)
                return Result::OutOfRange;
            position_ = offset;
            return Result::Success;
        }

        auto ReadNext(std::span<std::byte> destination) -> Result override {
            if (!RangeFits(position_, destination.size()))
                return Result::OutOfRange;
            if (destination.empty())
                return Result::Success;

            stream_.clear();
            stream_.seekg(static_cast<std::streamoff>(position_), std::ios::beg);
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
        uint64_t size_ = 0;
        uint64_t position_ = 0;
};

auto ReadAll(IFile& file, std::vector<std::byte>& output) -> Result {
    output.clear();
    const uint64_t size = file.Size();
    if (size > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max()))
        return Result::OutOfRange;
    output.resize(static_cast<std::size_t>(size));
    if (output.empty())
        return file.Seek(0);
    const Result result = file.ReadNext(output);
    if (result != Result::Success)
        output.clear();
    return result;
}

NativeFileSystem::NativeFileSystem(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
NativeFileSystem::~NativeFileSystem() = default;

auto NativeFileSystem::Create(const std::string_view mount, const std::string_view root_utf8,
                              std::unique_ptr<NativeFileSystem>& output) -> Result {
    output.reset();
    if (root_utf8.empty() || !atom::IsValidUtf8(std::string{root_utf8}))
        return Result::InvalidPath;

    AssetPath mountRoot{};
    if (!AssetPath::TryParse(std::string{mount} + "://", mountRoot) || mountRoot.Mount() != mount)
        return Result::InvalidPath;

    std::error_code error;
    const native_fs::path source = atom::PathFromUtf8(std::string{root_utf8});
    const native_fs::path root = native_fs::weakly_canonical(source, error);
    if (error)
        return error == std::errc::no_such_file_or_directory ? Result::NotFound : Result::IoError;
    if (!native_fs::is_directory(root, error))
        return error ? Result::IoError : Result::NotDirectory;

    output = std::unique_ptr<NativeFileSystem>{new NativeFileSystem{std::make_unique<Impl>(std::string{mount}, root)}};
    return Result::Success;
}

auto NativeFileSystem::Stat(const AssetPath& path, FileInfo& output) const -> Result {
    native_fs::path nativePath;
    const Result resolved = impl_->Resolve(path, nativePath);
    if (resolved != Result::Success)
        return resolved;

    std::error_code error;
    const native_fs::file_status status = native_fs::status(nativePath, error);
    if (error)
        return error == std::errc::no_such_file_or_directory ? Result::NotFound : Result::IoError;
    if (native_fs::is_regular_file(status)) {
        output.type = EntryType::File;
        output.size = native_fs::file_size(nativePath, error);
        return error ? Result::IoError : Result::Success;
    }
    if (native_fs::is_directory(status)) {
        output = {EntryType::Directory, 0};
        return Result::Success;
    }
    return Result::NotFile;
}

auto NativeFileSystem::OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result {
    output.reset();
    FileInfo info{};
    const Result status = Stat(path, info);
    if (status != Result::Success)
        return status;
    if (info.type != EntryType::File)
        return Result::NotFile;

    native_fs::path nativePath;
    const Result resolved = impl_->Resolve(path, nativePath);
    if (resolved != Result::Success)
        return resolved;
    auto file = std::make_unique<NativeReadFile>(nativePath, info.size);
    if (!file->IsOpen())
        return Result::IoError;
    output = std::move(file);
    return Result::Success;
}

auto NativeFileSystem::List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result {
    output.clear();
    native_fs::path nativePath;
    const Result resolved = impl_->Resolve(directory, nativePath);
    if (resolved != Result::Success)
        return resolved;

    std::error_code error;
    if (!native_fs::is_directory(nativePath, error))
        return error ? Result::IoError : Result::NotDirectory;

    native_fs::directory_iterator iterator{nativePath, native_fs::directory_options::skip_permission_denied, error};
    if (error)
        return Result::IoError;
    for (const native_fs::directory_entry& entry : iterator) {
        std::error_code entryError;
        const native_fs::file_status status = entry.status(entryError);
        if (entryError || (!native_fs::is_regular_file(status) && !native_fs::is_directory(status)))
            continue;
        const std::string name = atom::PathToUtf8(entry.path().filename());
        if (name.empty())
            continue;

        FileInfo info{};
        info.type = native_fs::is_directory(status) ? EntryType::Directory : EntryType::File;
        if (info.type == EntryType::File) {
            info.size = entry.file_size(entryError);
            if (entryError)
                continue;
        }
        output.push_back({name, info});
    }
    std::ranges::sort(output, {}, &DirectoryEntry::name);
    return Result::Success;
}

} // namespace atom::fs

/**
 * @file           : FileSystem.hpp
 * @brief          : Backend-neutral, read-only filesystem contracts.
 * @attention      : This public header intentionally does not expose
 *                   std::filesystem or platform-native path types.
**/

#ifndef ATOM_FILESYSTEM_FILE_SYSTEM_HPP
#define ATOM_FILESYSTEM_FILE_SYSTEM_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Filesystem/AssetPath.hpp>

namespace atom::fs {

enum class Result : uint8_t {
        Success,
        InvalidPath,
        NotFound,
        NotFile,
        NotDirectory,
        OutsideRoot,
        OutOfRange,
        IoError,
};

enum class EntryType : uint8_t { File, Directory };

struct FileInfo {
        EntryType type = EntryType::File;
        uint64_t size = 0;
};

struct DirectoryEntry {
        std::string name{};
        FileInfo info{};
};

// A file is independent from its filesystem's directory iterator state. A
// caller must not issue concurrent reads on the same IFile instance.
class IFile {
    public:
        virtual ~IFile() = default;

        [[nodiscard]] virtual auto Size() const -> uint64_t = 0;
        virtual auto ReadAt(uint64_t offset, std::span<std::byte> destination) -> Result = 0;
};

class IFileSystem {
    public:
        virtual ~IFileSystem() = default;

        [[nodiscard]] virtual auto Stat(const AssetPath& path, FileInfo& output) const -> Result = 0;
        virtual auto OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result = 0;
        virtual auto List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result = 0;
};

// Read-only filesystem rooted at one native directory. Every resolved asset
// path is checked after canonicalization, preventing a virtual path or a
// symlink from escaping the configured root.
class NativeFileSystem final : public IFileSystem {
    public:
        [[nodiscard]] static auto Create(std::string_view mount, std::string_view root_utf8,
                                         std::unique_ptr<NativeFileSystem>& output) -> Result;
        ~NativeFileSystem() override;

        NativeFileSystem(const NativeFileSystem&) = delete;
        NativeFileSystem& operator=(const NativeFileSystem&) = delete;

        [[nodiscard]] auto Stat(const AssetPath& path, FileInfo& output) const -> Result override;
        auto OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result override;
        auto List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result override;

    private:
        class Impl;
        explicit NativeFileSystem(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl_{};
};

} // namespace atom::fs

#endif // ATOM_FILESYSTEM_FILE_SYSTEM_HPP

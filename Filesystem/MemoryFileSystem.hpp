/**
 * @file           : MemoryFileSystem.hpp
 * @brief          : In-memory read/write filesystem for tests and generated assets.
**/

#ifndef ATOM_FILESYSTEM_MEMORY_FILE_SYSTEM_HPP
#define ATOM_FILESYSTEM_MEMORY_FILE_SYSTEM_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Filesystem/FileSystem.hpp>

namespace atom::fs {

// Stores files and synthetic directories under one mount. Open files share
// ownership of their byte buffers, so overwriting or removing a path does not
// invalidate an already-open IFile.
class MemoryFileSystem final : public IFileSystem {
    public:
        [[nodiscard]] static auto Create(std::string_view mount, std::unique_ptr<MemoryFileSystem>& output) -> Result;
        ~MemoryFileSystem() override;

        MemoryFileSystem(const MemoryFileSystem&) = delete;
        MemoryFileSystem& operator=(const MemoryFileSystem&) = delete;

        auto WriteFile(const AssetPath& path, std::span<const std::byte> data) -> Result;
        auto CreateDirectory(const AssetPath& path) -> Result;
        auto Remove(const AssetPath& path) -> Result;

        [[nodiscard]] auto Stat(const AssetPath& path, FileInfo& output) const -> Result override;
        auto OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result override;
        auto List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result override;

    private:
        class Impl;
        explicit MemoryFileSystem(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl_{};
};

} // namespace atom::fs

#endif // ATOM_FILESYSTEM_MEMORY_FILE_SYSTEM_HPP

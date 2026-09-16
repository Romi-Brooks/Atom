/**
 * @file           : PackageFileSystem.hpp
 * @brief          : Read-only APKG v1 package backend.
**/

#ifndef ATOM_FILESYSTEM_PACKAGE_FILE_SYSTEM_HPP
#define ATOM_FILESYSTEM_PACKAGE_FILE_SYSTEM_HPP

#include <memory>
#include <string_view>

#include <Filesystem/FileSystem.hpp>

namespace atom::fs {

// Read-only view over an APKG v1 archive (magic "APKG", trailing file table).
// Paths inside the package use '/' separators. Directory entries are synthesized
// from file path prefixes because APKG v1 has no directory table.
class PackageFileSystem final : public IFileSystem {
    public:
        [[nodiscard]] static auto Create(std::string_view mount, std::string_view package_utf8,
                                         std::unique_ptr<PackageFileSystem>& output) -> Result;
        ~PackageFileSystem() override;

        PackageFileSystem(const PackageFileSystem&) = delete;
        PackageFileSystem& operator=(const PackageFileSystem&) = delete;

        [[nodiscard]] auto Stat(const AssetPath& path, FileInfo& output) const -> Result override;
        auto OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result override;
        auto List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result override;

    private:
        class Impl;
        explicit PackageFileSystem(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl_{};
};

} // namespace atom::fs

#endif // ATOM_FILESYSTEM_PACKAGE_FILE_SYSTEM_HPP

/**
 * @file           : PackageFileSystem.hpp
 * @brief          : Read-only APKG v1 package backend.
**/

#ifndef ATOM_FILESYSTEM_PACKAGE_FILE_SYSTEM_HPP
#define ATOM_FILESYSTEM_PACKAGE_FILE_SYSTEM_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <Filesystem/FileSystem.hpp>

namespace atom::fs {

// One packaged asset's classification metadata, for tooling that inspects a
// package (e.g. the packager GUI). v1 archives have no kind: EnumerateAssets
// reports asset_kind::unknown for them.
struct PackagedAssetInfo {
        std::string path{};   // package-relative path, '/' separated
        std::uint16_t kind = 0; // mirrors atom::asset::AssetKind
        std::uint64_t size = 0;
        std::uint64_t content_hash = 0;
};

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

        // Flattens every packaged asset (not directories) with its kind, size
        // and content hash. Sorted by path. v1 archives report unknown kinds.
        [[nodiscard]] auto EnumerateAssets(std::vector<PackagedAssetInfo>& output) const -> Result;

    private:
        class Impl;
        explicit PackageFileSystem(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl_{};
};

} // namespace atom::fs

#endif // ATOM_FILESYSTEM_PACKAGE_FILE_SYSTEM_HPP

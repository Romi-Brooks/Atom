/**
 * @file           : OverlayFileSystem.hpp
 * @brief          : Priority overlay over multiple filesystem backends.
**/

#ifndef ATOM_FILESYSTEM_OVERLAY_FILE_SYSTEM_HPP
#define ATOM_FILESYSTEM_OVERLAY_FILE_SYSTEM_HPP

#include <memory>
#include <string_view>
#include <vector>

#include <Filesystem/FileSystem.hpp>

namespace atom::fs {

// Open and Stat use the first backend that can resolve the path. List merges
// directory entries; a name present in a higher-priority backend overwrites the
// same name from a lower-priority backend.
class OverlayFileSystem final : public IFileSystem {
    public:
        // Backends are tried in the given order (index 0 is highest priority).
        [[nodiscard]] static auto Create(std::vector<std::shared_ptr<IFileSystem>> backends_highest_first,
                                         std::unique_ptr<OverlayFileSystem>& output) -> Result;
        ~OverlayFileSystem() override;

        OverlayFileSystem(const OverlayFileSystem&) = delete;
        OverlayFileSystem& operator=(const OverlayFileSystem&) = delete;

        [[nodiscard]] auto Stat(const AssetPath& path, FileInfo& output) const -> Result override;
        auto OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result override;
        auto List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result override;

    private:
        explicit OverlayFileSystem(std::vector<std::shared_ptr<IFileSystem>> backends);

        std::vector<std::shared_ptr<IFileSystem>> backends_{};
};

} // namespace atom::fs

#endif // ATOM_FILESYSTEM_OVERLAY_FILE_SYSTEM_HPP

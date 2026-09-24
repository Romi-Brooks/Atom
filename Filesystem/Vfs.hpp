/**
 * @file           : Vfs.hpp
 * @brief          : Named mount table that routes AssetPath queries to backends.
**/

#ifndef ATOM_FILESYSTEM_VFS_HPP
#define ATOM_FILESYSTEM_VFS_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <Filesystem/FileSystem.hpp>

namespace atom::fs {

// Mount table keyed by AssetPath mount name. Multiple backends may share one
// mount; higher priority wins for Open/Stat, and List merges by priority.
//
// Mount and Unmount take effect immediately. Callers that need frame-boundary
// safety must invoke these methods from a known safe point until CORE-001
// defines a first-class frame semantic.
class Vfs final : public IFileSystem {
    public:
        Vfs() = default;
        ~Vfs() override;

        Vfs(const Vfs&) = delete;
        Vfs& operator=(const Vfs&) = delete;

        // Process-wide default Vfs. String convenience entry points (e.g.
        // MusicPlayer::Load(id, "res://...")) resolve through this instance, so
        // callers that only ever pass asset-path strings never have to hold a
        // Vfs handle. Configure it once at startup with Mount(); tests and tools
        // that need isolation can still construct a local Vfs and pass it
        // explicitly to the IFileSystem overloads.
        static auto GetInstance() -> Vfs&;

        auto Mount(std::string_view mount, int priority, std::shared_ptr<IFileSystem> backend) -> Result;
        auto Unmount(std::string_view mount, const IFileSystem* backend) -> Result;
        auto UnmountAll(std::string_view mount) -> Result;

        [[nodiscard]] auto MountCount(std::string_view mount) const -> std::size_t;

        [[nodiscard]] auto Stat(const AssetPath& path, FileInfo& output) const -> Result override;
        auto OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result override;
        auto List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result override;

    private:
        struct MountEntry {
                int priority = 0;
                std::shared_ptr<IFileSystem> backend{};
        };

        [[nodiscard]] auto BackendsFor(std::string_view mount) const -> std::vector<std::shared_ptr<IFileSystem>>;

        // Sorted by mount name; within a mount, highest priority first.
        std::vector<std::pair<std::string, std::vector<MountEntry>>> mounts_{};
};

} // namespace atom::fs

#endif // ATOM_FILESYSTEM_VFS_HPP

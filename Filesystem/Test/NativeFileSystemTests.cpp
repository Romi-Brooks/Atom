#include <Filesystem/FileSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

class TemporaryDirectory final {
    public:
        [[nodiscard]] static auto Create(TemporaryDirectory& output) -> bool {
            std::error_code error;
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto root = std::filesystem::temp_directory_path(error) /
                              ("atom_fs_native_test_" + std::to_string(nonce));
            if (error || std::filesystem::exists(root, error))
                return false;
            if (!std::filesystem::create_directories(root / "textures", error) || error)
                return false;
            output.root_ = root;
            return true;
        }

        ~TemporaryDirectory() {
            if (!root_.empty()) {
                std::error_code error;
                std::filesystem::remove_all(root_, error);
            }
        }

        TemporaryDirectory() = default;
        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        [[nodiscard]] auto Root() const -> const std::filesystem::path& { return root_; }

    private:
        std::filesystem::path root_{};
};

auto Fail(const std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

auto main() -> int {
    TemporaryDirectory temporary{};
    if (!TemporaryDirectory::Create(temporary))
        return Fail("Could not create test directory");

    {
        std::ofstream file{temporary.Root() / "textures" / "button.txt", std::ios::binary};
        file << "Atom";
        if (!file)
            return Fail("Could not create test file");
    }

    std::unique_ptr<atom::fs::NativeFileSystem> filesystem{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(temporary.Root()), filesystem) !=
            atom::fs::Result::Success ||
        !filesystem) {
        return Fail("Could not create native filesystem");
    }

    atom::fs::AssetPath asset{};
    atom::fs::AssetPath wrongMount{};
    atom::fs::AssetPath directory{};
    if (!atom::fs::AssetPath::TryParse("res://textures/button.txt", asset) ||
        !atom::fs::AssetPath::TryParse("engine://textures/button.txt", wrongMount) ||
        !atom::fs::AssetPath::TryParse("res://textures", directory)) {
        return Fail("Could not parse test asset paths");
    }

    atom::fs::FileInfo info{};
    if (filesystem->Stat(asset, info) != atom::fs::Result::Success || info.type != atom::fs::EntryType::File ||
        info.size != 4) {
        return Fail("Incorrect file stat");
    }
    if (filesystem->Stat(wrongMount, info) != atom::fs::Result::NotFound)
        return Fail("Filesystem accepted a different mount");

    std::unique_ptr<atom::fs::IFile> file{};
    if (filesystem->OpenRead(asset, file) != atom::fs::Result::Success || !file)
        return Fail("Could not open test file");
    std::array<std::byte, 4> bytes{};
    if (file->ReadAt(0, bytes) != atom::fs::Result::Success ||
        std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()} != "Atom") {
        return Fail("Incorrect file contents");
    }
    if (file->ReadAt(4, std::span<std::byte>{bytes}.first(1)) != atom::fs::Result::OutOfRange)
        return Fail("Out-of-range read was accepted");

    std::vector<atom::fs::DirectoryEntry> entries{};
    if (filesystem->List(directory, entries) != atom::fs::Result::Success || entries.size() != 1 ||
        entries.front().name != "button.txt") {
        return Fail("Incorrect directory entries");
    }
    return 0;
}

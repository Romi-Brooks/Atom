/**
  * @file           : PackageBuilderTest.cpp
  * @brief          : APKG v2 builder <-> reader round-trip tests.
  * @attention      : The writer (PackageBuilder, Atom_Packager) and the reader
  *                   (PackageFileSystem, Atom_FS) only share the header-only
  *                   contract in Filesystem/PackageFormatV2.hpp.
**/

#include <Test/Support/TestHelpers.hpp>
#include <Utilities/Packager/PackageBuilder.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Filesystem/PackageFileSystem.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace native_fs = std::filesystem;

class TemporaryDirectory final {
    public:
        [[nodiscard]] static auto Create(TemporaryDirectory& output) -> bool {
            std::error_code error;
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto root = native_fs::temp_directory_path(error) / ("atom_apkg_v2_test_" + std::to_string(nonce));
            if (error || native_fs::exists(root, error))
                return false;
            if (!native_fs::create_directories(root, error) || error)
                return false;
            output.root_ = root;
            return true;
        }

        ~TemporaryDirectory() {
            if (!root_.empty()) {
                std::error_code error;
                native_fs::remove_all(root_, error);
            }
        }

        TemporaryDirectory() = default;
        TemporaryDirectory(const TemporaryDirectory&) = delete;
        auto operator=(const TemporaryDirectory&) -> TemporaryDirectory& = delete;

        [[nodiscard]] auto Root() const -> const native_fs::path& { return root_; }

    private:
        native_fs::path root_{};
};

auto WriteText(const native_fs::path& path, const std::string& text) -> bool {
    std::ofstream stream(path, std::ios::binary);
    if (!stream)
        return false;
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

auto ReadFileBytes(const native_fs::path& path, std::vector<std::byte>& output) -> bool {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return false;
    const auto end = stream.tellg();
    if (end < 0)
        return false;
    output.resize(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (output.empty())
        return true;
    stream.read(reinterpret_cast<char*>(output.data()), end);
    return static_cast<bool>(stream);
}

// Corrupts one byte at an absolute offset inside a copy of the package.
auto CorruptByte(const native_fs::path& source, const native_fs::path& destination, const std::uint64_t offset) -> bool {
    std::vector<std::byte> bytes;
    if (!ReadFileBytes(source, bytes) || offset >= bytes.size())
        return false;
    bytes[offset] = std::byte(std::to_integer<unsigned>(bytes[offset]) ^ 0xFF);
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    if (!stream)
        return false;
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

auto ReadAllToString(atom::fs::IFile& file, std::string& output) -> bool {
    std::vector<std::byte> bytes;
    if (atom::fs::ReadAll(file, bytes) != atom::fs::Result::Success)
        return false;
    output.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

} // namespace

auto main() -> int {
    TemporaryDirectory temp{};
    ATOM_CHECK(TemporaryDirectory::Create(temp));

    // ---- Fixture: nested input tree ----
    const auto input_dir = temp.Root() / "in";
    std::error_code error{};
    native_fs::create_directories(input_dir / "textures" / "ui", error);
    ATOM_CHECK(!error);
    native_fs::create_directories(input_dir / "audio", error);
    ATOM_CHECK(!error);
    ATOM_CHECK(WriteText(input_dir / "root.txt", "root-level"));
    ATOM_CHECK(WriteText(input_dir / "textures" / "ui" / "button.png", "\x89PNG-fake-bytes"));
    ATOM_CHECK(WriteText(input_dir / "textures" / "bg.png", "background-image"));
    ATOM_CHECK(WriteText(input_dir / "audio" / "theme.wav", "RIFF-fake-wave"));
    // A file large enough to exercise the streaming write path (> 64 KiB).
    std::string big(200 * 1024, '\0');
    for (std::size_t i = 0; i < big.size(); ++i)
        big[i] = static_cast<char>((i * 31 + 7) & 0xFF);
    ATOM_CHECK(WriteText(input_dir / "audio" / "big.raw", big));

    const auto package_path = temp.Root() / "game.apkg";

    // ---- Builder validation failures happen before any write ----
    {
        atom::tools::PackageBuilder builder{};
        ATOM_CHECK(builder.AddDirectory(input_dir.string()) == atom::tools::PackageBuilder::Result::Success);
        ATOM_CHECK(builder.AddFile((input_dir / "root.txt").string(), "textures/bg.png") ==
                   atom::tools::PackageBuilder::Result::Success);
        ATOM_CHECK(builder.Build(package_path.string()) == atom::tools::PackageBuilder::Result::ErrorDuplicatePath);
        ATOM_CHECK(!native_fs::exists(package_path));
    }
    {
        atom::tools::PackageBuilder builder{};
        ATOM_CHECK(builder.AddFile((input_dir / "root.txt").string(), "../escape.txt") ==
                   atom::tools::PackageBuilder::Result::ErrorInvalidPath);
        ATOM_CHECK(builder.AddFile((input_dir / "root.txt").string(), "textures\\win.png") ==
                   atom::tools::PackageBuilder::Result::ErrorInvalidPath);
        ATOM_CHECK(builder.AddFile((input_dir / "missing.bin").string(), "missing.bin") ==
                   atom::tools::PackageBuilder::Result::ErrorInvalidPath);
        ATOM_CHECK(builder.Build(package_path.string()) == atom::tools::PackageBuilder::Result::ErrorEmptyPackage);
        ATOM_CHECK(!native_fs::exists(package_path));
    }

    // ---- Build a valid package ----
    {
        atom::tools::PackageBuilder builder{};
        ATOM_CHECK(builder.AddDirectory(input_dir.string()) == atom::tools::PackageBuilder::Result::Success);
        ATOM_CHECK(builder.PlannedFiles().size() == 5);
        ATOM_CHECK(builder.Build(package_path.string()) == atom::tools::PackageBuilder::Result::Success);
        ATOM_CHECK(native_fs::exists(package_path));
        // Temp file must not survive a successful build.
        ATOM_CHECK(!native_fs::exists(package_path.string() + ".tmp"));
    }

    // Deterministic: identical inputs produce a byte-identical package.
    {
        const auto second_path = temp.Root() / "game2.apkg";
        atom::tools::PackageBuilder builder{};
        ATOM_CHECK(builder.AddDirectory(input_dir.string()) == atom::tools::PackageBuilder::Result::Success);
        ATOM_CHECK(builder.Build(second_path.string()) == atom::tools::PackageBuilder::Result::Success);
        std::vector<std::byte> first_bytes;
        std::vector<std::byte> second_bytes;
        ATOM_CHECK(ReadFileBytes(package_path, first_bytes));
        ATOM_CHECK(ReadFileBytes(second_path, second_bytes));
        ATOM_CHECK(first_bytes == second_bytes);
    }

    // ---- Round-trip through the v2 reader ----
    {
        std::unique_ptr<atom::fs::PackageFileSystem> package{};
        ATOM_CHECK(atom::fs::PackageFileSystem::Create("res", package_path.string(), package) ==
                   atom::fs::Result::Success);

        atom::fs::AssetPath path{};
        ATOM_CHECK(atom::fs::AssetPath::TryParse("res://root.txt", path));
        atom::fs::FileInfo info{};
        ATOM_CHECK(package->Stat(path, info) == atom::fs::Result::Success);
        ATOM_CHECK(info.type == atom::fs::EntryType::File);
        ATOM_CHECK(info.size == std::string("root-level").size());

        std::unique_ptr<atom::fs::IFile> file{};
        ATOM_CHECK(package->OpenRead(path, file) == atom::fs::Result::Success);
        std::string content;
        ATOM_CHECK(ReadAllToString(*file, content));
        ATOM_CHECK(content == "root-level");

        // Nested file, exact content.
        ATOM_CHECK(atom::fs::AssetPath::TryParse("res://textures/ui/button.png", path));
        ATOM_CHECK(package->OpenRead(path, file) == atom::fs::Result::Success);
        ATOM_CHECK(ReadAllToString(*file, content));
        ATOM_CHECK(content == "\x89PNG-fake-bytes");

        // Large file streams back byte-identical.
        ATOM_CHECK(atom::fs::AssetPath::TryParse("res://audio/big.raw", path));
        ATOM_CHECK(package->Stat(path, info) == atom::fs::Result::Success);
        ATOM_CHECK(info.size == big.size());
        ATOM_CHECK(package->OpenRead(path, file) == atom::fs::Result::Success);
        ATOM_CHECK(file->Size() == big.size());
        ATOM_CHECK(ReadAllToString(*file, content));
        ATOM_CHECK(content == big);

        // ReadAt across the 64 KiB write-block boundary.
        std::vector<std::byte> tail(100);
        ATOM_CHECK(file->Seek(big.size() - 100) == atom::fs::Result::Success);
        ATOM_CHECK(file->ReadNext(tail) == atom::fs::Result::Success);
        ATOM_CHECK(std::memcmp(tail.data(), big.data() + big.size() - 100, 100) == 0);

        // Directory stat + real directory listing (no prefix synthesis).
        ATOM_CHECK(atom::fs::AssetPath::TryParse("res://textures", path));
        ATOM_CHECK(package->Stat(path, info) == atom::fs::Result::Success);
        ATOM_CHECK(info.type == atom::fs::EntryType::Directory);

        std::vector<atom::fs::DirectoryEntry> listing{};
        ATOM_CHECK(package->List(path, listing) == atom::fs::Result::Success);
        ATOM_CHECK(listing.size() == 2); // bg.png + ui/
        bool saw_bg = false;
        bool saw_ui_dir = false;
        for (const auto& entry : listing) {
            if (entry.name == "bg.png") {
                saw_bg = true;
                ATOM_CHECK(entry.info.type == atom::fs::EntryType::File);
            }
            if (entry.name == "ui") {
                saw_ui_dir = true;
                ATOM_CHECK(entry.info.type == atom::fs::EntryType::Directory);
            }
        }
        ATOM_CHECK(saw_bg && saw_ui_dir);

        ATOM_CHECK(atom::fs::AssetPath::TryParse("res://", path));
        ATOM_CHECK(package->List(path, listing) == atom::fs::Result::Success);
        ATOM_CHECK(listing.size() == 3); // root.txt + textures/ + audio/

        // Missing entries stay missing.
        ATOM_CHECK(atom::fs::AssetPath::TryParse("res://no/such.txt", path));
        ATOM_CHECK(package->Stat(path, info) == atom::fs::Result::NotFound);
    }

    // ---- Corruption is detected ----
    {
        const auto corrupt_path = temp.Root() / "corrupt.apkg";

        // Flip a byte inside the fixed header (uuid field, offset 8).
        ATOM_CHECK(CorruptByte(package_path, corrupt_path, 8));
        std::unique_ptr<atom::fs::PackageFileSystem> package{};
        ATOM_CHECK(atom::fs::PackageFileSystem::Create("res", corrupt_path.string(), package) !=
                   atom::fs::Result::Success);

        // Flip a byte near the end (inside the manifest).
        std::vector<std::byte> bytes;
        ATOM_CHECK(ReadFileBytes(package_path, bytes));
        ATOM_CHECK(CorruptByte(package_path, corrupt_path, bytes.size() - 4));
        ATOM_CHECK(atom::fs::PackageFileSystem::Create("res", corrupt_path.string(), package) !=
                   atom::fs::Result::Success);

        // Bad magic is rejected outright.
        ATOM_CHECK(WriteText(corrupt_path, "NOPE-this-is-not-a-package-at-all....................................."
                                           "...................................................................."));
        ATOM_CHECK(atom::fs::PackageFileSystem::Create("res", corrupt_path.string(), package) !=
                   atom::fs::Result::Success);
    }

    // ---- No-overwrite config refuses to replace an existing package ----
    {
        atom::tools::PackageBuilder builder{};
        ATOM_CHECK(builder.AddDirectory(input_dir.string()) == atom::tools::PackageBuilder::Result::Success);
        atom::tools::PackageBuilder::Config config{};
        config.overwrite = false;
        ATOM_CHECK(builder.Build(package_path.string(), config) == atom::tools::PackageBuilder::Result::ErrorWriteFailed);
    }

    return 0;
}

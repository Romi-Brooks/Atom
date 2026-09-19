#include <Filesystem/PackageFileSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace native_fs = std::filesystem;

auto Fail(const std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

auto Text(const std::vector<std::byte>& bytes) -> std::string {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

class TemporaryDirectory final {
    public:
        [[nodiscard]] static auto Create(TemporaryDirectory& output) -> bool {
            std::error_code error;
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto root = native_fs::temp_directory_path(error) / ("atom_fs_package_test_" + std::to_string(nonce));
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
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        [[nodiscard]] auto Root() const -> const native_fs::path& {
            return root_;
        }

    private:
        native_fs::path root_{};
};

// Minimal APKG v1 writer used only by this test. Mirrors Utilities/Packager.
auto WriteApkg(const native_fs::path& package, const std::vector<std::pair<std::string, std::string>>& files) -> bool {
    std::ofstream output{package, std::ios::binary | std::ios::trunc};
    if (!output.is_open())
        return false;

    output.write("APKG", 4);
    const uint16_t version = 1;
    output.write(reinterpret_cast<const char*>(&version), sizeof(version));
    const uint32_t count = static_cast<uint32_t>(files.size());
    output.write(reinterpret_cast<const char*>(&count), sizeof(count));

    struct Row {
            std::string name{};
            uint64_t offset = 0;
            uint64_t size = 0;
    };
    std::vector<Row> rows{};
    rows.reserve(files.size());
    for (const auto& [name, text] : files) {
        Row row{name, static_cast<uint64_t>(output.tellp()), text.size()};
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        rows.push_back(std::move(row));
    }

    const auto table_offset = static_cast<uint64_t>(output.tellp());
    for (const auto& row : rows) {
        const auto name_length = static_cast<uint16_t>(row.name.size());
        output.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));
        output.write(row.name.data(), name_length);
        output.write(reinterpret_cast<const char*>(&row.offset), sizeof(row.offset));
        output.write(reinterpret_cast<const char*>(&row.size), sizeof(row.size));
        const uint8_t type_length = 0;
        output.write(reinterpret_cast<const char*>(&type_length), sizeof(type_length));
    }
    output.write(reinterpret_cast<const char*>(&table_offset), sizeof(table_offset));
    return static_cast<bool>(output);
}

} // namespace

auto main() -> int {
    TemporaryDirectory temporary{};
    if (!TemporaryDirectory::Create(temporary))
        return Fail("Could not create temporary directory");

    const native_fs::path package = temporary.Root() / "game.apkg";
    if (!WriteApkg(package, {
                                {"textures/ui/button.txt", "Atom"},
                                {"textures/ui/icon.txt", "Icon"},
                                {"config/default.txt", "cfg"},
                            })) {
        return Fail("Could not write test package");
    }

    std::unique_ptr<atom::fs::PackageFileSystem> packaged{};
    const std::string package_utf8 = atom::PathToUtf8(package);
    if (atom::fs::PackageFileSystem::Create("res", package_utf8, packaged) != atom::fs::Result::Success || !packaged)
        return Fail("Could not open package filesystem");

    atom::fs::AssetPath button{};
    atom::fs::AssetPath wrongMount{};
    atom::fs::AssetPath ui{};
    atom::fs::AssetPath root{};
    if (!atom::fs::AssetPath::TryParse("res://textures/ui/button.txt", button) ||
        !atom::fs::AssetPath::TryParse("engine://textures/ui/button.txt", wrongMount) ||
        !atom::fs::AssetPath::TryParse("res://textures/ui", ui) || !atom::fs::AssetPath::TryParse("res://", root)) {
        return Fail("Could not parse package paths");
    }

    atom::fs::FileInfo info{};
    if (packaged->Stat(button, info) != atom::fs::Result::Success || info.type != atom::fs::EntryType::File ||
        info.size != 4) {
        return Fail("Incorrect packaged file stat");
    }
    if (packaged->Stat(ui, info) != atom::fs::Result::Success || info.type != atom::fs::EntryType::Directory)
        return Fail("Packaged directory was not synthesized");
    if (packaged->Stat(wrongMount, info) != atom::fs::Result::NotFound)
        return Fail("Package filesystem accepted a different mount");

    std::unique_ptr<atom::fs::IFile> file{};
    if (packaged->OpenRead(button, file) != atom::fs::Result::Success || !file)
        return Fail("Could not open packaged file");
    std::vector<std::byte> bytes(file->Size());
    if (file->ReadNext(bytes) != atom::fs::Result::Success || Text(bytes) != "Atom")
        return Fail("Incorrect packaged file contents");
    if (file->Seek(2) != atom::fs::Result::Success)
        return Fail("Packaged seek failed");
    std::vector<std::byte> tail(2);
    if (file->ReadNext(tail) != atom::fs::Result::Success || Text(tail) != "om")
        return Fail("Packaged sequential read failed");

    std::vector<atom::fs::DirectoryEntry> entries{};
    if (packaged->List(root, entries) != atom::fs::Result::Success || entries.size() != 2)
        return Fail("Incorrect package root listing");
    if (packaged->List(ui, entries) != atom::fs::Result::Success || entries.size() != 2 ||
        entries[0].name != "button.txt" || entries[1].name != "icon.txt") {
        return Fail("Incorrect package ui listing");
    }

    if (atom::fs::PackageFileSystem::Create("res", package_utf8 + ".missing", packaged) == atom::fs::Result::Success) {
        return Fail("Package filesystem accepted a missing file");
    }
    return 0;
}

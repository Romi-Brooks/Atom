#include <Filesystem/AssetPath.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Filesystem/Vfs.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

auto MakeMemory(const std::string_view mount, std::shared_ptr<atom::fs::MemoryFileSystem>& output) -> bool {
    std::unique_ptr<atom::fs::MemoryFileSystem> owned{};
    if (atom::fs::MemoryFileSystem::Create(mount, owned) != atom::fs::Result::Success || !owned)
        return false;
    output = std::shared_ptr<atom::fs::MemoryFileSystem>{std::move(owned)};
    return true;
}

auto ReadText(const atom::fs::IFileSystem& fs, const std::string_view uri) -> std::string {
    atom::fs::AssetPath path{};
    if (!atom::fs::AssetPath::TryParse(uri, path))
        return {};
    std::unique_ptr<atom::fs::IFile> file{};
    if (fs.OpenRead(path, file) != atom::fs::Result::Success || !file)
        return {};
    std::vector<std::byte> data{};
    if (atom::fs::ReadAll(*file, data) != atom::fs::Result::Success)
        return {};
    return {reinterpret_cast<const char*>(data.data()), data.size()};
}

} // namespace

auto main() -> int {
    using namespace atom::fs;

    std::shared_ptr<MemoryFileSystem> low{};
    std::shared_ptr<MemoryFileSystem> high{};
    ATOM_CHECK(MakeMemory("res", low));
    ATOM_CHECK(MakeMemory("res", high));

    atom::fs::AssetPath config{};
    atom::fs::AssetPath only_low{};
    ATOM_CHECK(AssetPath::TryParse("res://config/app.txt", config));
    ATOM_CHECK(AssetPath::TryParse("res://only/low.txt", only_low));
    ATOM_CHECK(low->WriteFile(config, Bytes("from-low")) == Result::Success);
    ATOM_CHECK(low->WriteFile(only_low, Bytes("low-only")) == Result::Success);
    ATOM_CHECK(high->WriteFile(config, Bytes("from-high")) == Result::Success);

    Vfs vfs{};
    ATOM_CHECK(vfs.MountCount("res") == 0);
    ATOM_CHECK(vfs.Mount("res", 10, low) == Result::Success);
    ATOM_CHECK(vfs.Mount("res", 20, high) == Result::Success);
    ATOM_CHECK(vfs.MountCount("res") == 2);
    ATOM_CHECK(ReadText(vfs, "res://config/app.txt") == "from-high");
    ATOM_CHECK(ReadText(vfs, "res://only/low.txt") == "low-only");

    ATOM_CHECK(vfs.Mount("bad mount", 1, low) != Result::Success);
    ATOM_CHECK(vfs.Mount("res", 5, nullptr) != Result::Success);

    ATOM_CHECK(vfs.Unmount("res", high.get()) == Result::Success);
    ATOM_CHECK(vfs.MountCount("res") == 1);
    ATOM_CHECK(ReadText(vfs, "res://config/app.txt") == "from-low");

    ATOM_CHECK(vfs.Mount("res", 99, high) == Result::Success);
    ATOM_CHECK(vfs.MountCount("res") == 2);
    ATOM_CHECK(vfs.UnmountAll("res") == Result::Success);
    ATOM_CHECK(vfs.MountCount("res") == 0);

    std::unique_ptr<IFile> missing{};
    AssetPath missing_path{};
    ATOM_CHECK(AssetPath::TryParse("res://gone.txt", missing_path));
    ATOM_CHECK(vfs.OpenRead(missing_path, missing) != Result::Success);
    return 0;
}

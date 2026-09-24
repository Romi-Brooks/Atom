#include <Filesystem/MemoryFileSystem.hpp>

#include <array>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

auto Fail(const std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

auto Text(const std::span<const std::byte> bytes) -> std::string {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

} // namespace

auto main() -> int {
    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return Fail("Could not create memory filesystem");

    atom::fs::AssetPath button{};
    atom::fs::AssetPath wrongMount{};
    atom::fs::AssetPath textures{};
    if (!atom::fs::AssetPath::TryParse("res://textures/ui/button.txt", button) ||
        !atom::fs::AssetPath::TryParse("engine://textures/ui/button.txt", wrongMount) ||
        !atom::fs::AssetPath::TryParse("res://textures", textures)) {
        return Fail("Could not parse test paths");
    }

    const auto payload = Bytes("Atom");
    if (memory->WriteFile(button, payload) != atom::fs::Result::Success)
        return Fail("Could not write memory file");

    atom::fs::FileInfo info{};
    if (memory->Stat(button, info) != atom::fs::Result::Success || info.type != atom::fs::EntryType::File ||
        info.size != 4) {
        return Fail("Incorrect memory file stat");
    }
    if (memory->Stat(textures, info) != atom::fs::Result::Success || info.type != atom::fs::EntryType::Directory)
        return Fail("Ancestor directory was not synthesized");
    if (memory->Stat(wrongMount, info) != atom::fs::Result::NotFound)
        return Fail("Memory filesystem accepted a different mount");

    std::unique_ptr<atom::fs::IFile> file{};
    if (memory->OpenRead(button, file) != atom::fs::Result::Success || !file)
        return Fail("Could not open memory file");
    std::array<std::byte, 2> head{};
    if (file->ReadNext(head) != atom::fs::Result::Success || Text(head) != "At")
        return Fail("Sequential read failed");
    if (file->Tell() != 2)
        return Fail("Tell did not advance");
    std::array<std::byte, 2> tail{};
    if (file->ReadNext(tail) != atom::fs::Result::Success || Text(tail) != "om")
        return Fail("Second sequential read failed");
    if (file->Seek(1) != atom::fs::Result::Success)
        return Fail("Seek failed");
    std::array<std::byte, 3> mid{};
    if (file->ReadNext(mid) != atom::fs::Result::Success || Text(mid) != "tom")
        return Fail("Read after seek failed");

    // Overwrite must not invalidate an already-open file.
    if (memory->WriteFile(button, Bytes("Changed")) != atom::fs::Result::Success)
        return Fail("Could not overwrite memory file");
    if (file->Seek(0) != atom::fs::Result::Success || file->ReadNext(head) != atom::fs::Result::Success ||
        Text(head) != "At") {
        return Fail("Open file observed overwrite");
    }

    std::vector<atom::fs::DirectoryEntry> entries{};
    if (memory->List(textures, entries) != atom::fs::Result::Success || entries.size() != 1 ||
        entries.front().name != "ui" || entries.front().info.type != atom::fs::EntryType::Directory) {
        return Fail("Incorrect textures listing");
    }

    atom::fs::AssetPath ui{};
    if (!atom::fs::AssetPath::TryParse("res://textures/ui", ui))
        return Fail("Could not parse ui path");
    if (memory->List(ui, entries) != atom::fs::Result::Success || entries.size() != 1 ||
        entries.front().name != "button.txt" || entries.front().info.size != 7) {
        return Fail("Incorrect ui listing");
    }

    if (memory->Remove(button) != atom::fs::Result::Success)
        return Fail("Could not remove memory file");
    if (memory->Stat(button, info) != atom::fs::Result::NotFound)
        return Fail("Removed file still exists");

    // Removing a directory must cascade to its descendants and not leave
    // orphaned children reachable under a vanished parent.
    atom::fs::AssetPath nested{};
    atom::fs::AssetPath nestedChild{};
    if (!atom::fs::AssetPath::TryParse("res://audio/music/track.mp3", nestedChild) ||
        !atom::fs::AssetPath::TryParse("res://audio", nested)) {
        return Fail("Could not parse nested removal paths");
    }
    if (memory->WriteFile(nestedChild, Bytes("samples")) != atom::fs::Result::Success)
        return Fail("Could not write nested file");
    if (memory->Remove(nested) != atom::fs::Result::Success)
        return Fail("Could not remove directory");
    if (memory->Stat(nested, info) != atom::fs::Result::NotFound)
        return Fail("Removed directory still exists");
    if (memory->Stat(nestedChild, info) != atom::fs::Result::NotFound)
        return Fail("Orphaned child survived directory removal");

    return 0;
}

#include <Filesystem/MemoryFileSystem.hpp>
#include <Filesystem/OverlayFileSystem.hpp>
#include <Filesystem/Vfs.hpp>

#include <iostream>
#include <memory>
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

auto Text(const std::vector<std::byte>& bytes) -> std::string {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

auto MakeMemory(const std::string_view mount, std::shared_ptr<atom::fs::MemoryFileSystem>& output) -> bool {
    std::unique_ptr<atom::fs::MemoryFileSystem> owned{};
    if (atom::fs::MemoryFileSystem::Create(mount, owned) != atom::fs::Result::Success || !owned)
        return false;
    output = std::shared_ptr<atom::fs::MemoryFileSystem>{std::move(owned)};
    return true;
}

} // namespace

auto main() -> int {
    std::shared_ptr<atom::fs::MemoryFileSystem> base{};
    std::shared_ptr<atom::fs::MemoryFileSystem> override_fs{};
    if (!MakeMemory("res", base) || !MakeMemory("res", override_fs))
        return Fail("Could not create memory backends");

    atom::fs::AssetPath shared{};
    atom::fs::AssetPath onlyBase{};
    atom::fs::AssetPath onlyOverride{};
    atom::fs::AssetPath root{};
    if (!atom::fs::AssetPath::TryParse("res://textures/button.txt", shared) ||
        !atom::fs::AssetPath::TryParse("res://textures/base.txt", onlyBase) ||
        !atom::fs::AssetPath::TryParse("res://textures/override.txt", onlyOverride) ||
        !atom::fs::AssetPath::TryParse("res://", root)) {
        return Fail("Could not parse test paths");
    }

    if (base->WriteFile(shared, Bytes("base")) != atom::fs::Result::Success ||
        base->WriteFile(onlyBase, Bytes("only-base")) != atom::fs::Result::Success ||
        override_fs->WriteFile(shared, Bytes("override")) != atom::fs::Result::Success ||
        override_fs->WriteFile(onlyOverride, Bytes("only-override")) != atom::fs::Result::Success) {
        return Fail("Could not seed memory backends");
    }

    std::unique_ptr<atom::fs::OverlayFileSystem> overlay{};
    if (atom::fs::OverlayFileSystem::Create({override_fs, base}, overlay) != atom::fs::Result::Success || !overlay)
        return Fail("Could not create overlay");

    std::unique_ptr<atom::fs::IFile> file{};
    if (overlay->OpenRead(shared, file) != atom::fs::Result::Success || !file)
        return Fail("Overlay could not open shared file");
    std::vector<std::byte> bytes(file->Size());
    if (file->ReadNext(bytes) != atom::fs::Result::Success || Text(bytes) != "override")
        return Fail("High priority backend did not win Open");

    std::vector<atom::fs::DirectoryEntry> entries{};
    if (overlay->List(atom::fs::AssetPath{}, entries) == atom::fs::Result::Success)
        return Fail("Overlay accepted an invalid path");

    atom::fs::AssetPath textures{};
    if (!atom::fs::AssetPath::TryParse("res://textures", textures))
        return Fail("Could not parse textures path");
    if (overlay->List(textures, entries) != atom::fs::Result::Success || entries.size() != 3)
        return Fail("Overlay list did not merge three names");
    bool sawButton = false;
    bool sawBase = false;
    bool sawOverride = false;
    for (const auto& entry : entries) {
        if (entry.name == "button.txt")
            sawButton = true;
        if (entry.name == "base.txt")
            sawBase = true;
        if (entry.name == "override.txt")
            sawOverride = true;
    }
    if (!sawButton || !sawBase || !sawOverride)
        return Fail("Overlay list missed expected entries");

    atom::fs::Vfs vfs{};
    std::shared_ptr<atom::fs::MemoryFileSystem> engine{};
    if (!MakeMemory("engine", engine))
        return Fail("Could not create engine backend");
    atom::fs::AssetPath engineShader{};
    if (!atom::fs::AssetPath::TryParse("engine://shaders/sprite.spv", engineShader))
        return Fail("Could not parse engine path");
    if (engine->WriteFile(engineShader, Bytes("spv")) != atom::fs::Result::Success)
        return Fail("Could not write engine file");

    if (vfs.Mount("res", 100, base) != atom::fs::Result::Success ||
        vfs.Mount("res", 200, override_fs) != atom::fs::Result::Success ||
        vfs.Mount("engine", 100, engine) != atom::fs::Result::Success) {
        return Fail("Could not mount vfs backends");
    }
    if (vfs.MountCount("res") != 2 || vfs.MountCount("engine") != 1)
        return Fail("Incorrect mount counts");

    if (vfs.OpenRead(shared, file) != atom::fs::Result::Success || !file)
        return Fail("Vfs could not open res file");
    bytes.assign(file->Size(), std::byte{});
    if (file->ReadNext(bytes) != atom::fs::Result::Success || Text(bytes) != "override")
        return Fail("Vfs priority is wrong");

    if (vfs.OpenRead(engineShader, file) != atom::fs::Result::Success || !file)
        return Fail("Vfs could not open engine file");
    if (vfs.List(textures, entries) != atom::fs::Result::Success || entries.size() != 3)
        return Fail("Vfs list merge failed");

    if (vfs.Unmount("res", override_fs.get()) != atom::fs::Result::Success)
        return Fail("Could not unmount override backend");
    if (vfs.OpenRead(shared, file) != atom::fs::Result::Success || !file)
        return Fail("Vfs lost base backend after unmount");
    bytes.assign(file->Size(), std::byte{});
    if (file->ReadNext(bytes) != atom::fs::Result::Success || Text(bytes) != "base")
        return Fail("Unmount did not fall back to base");
    if (vfs.MountCount("res") != 1)
        return Fail("Mount count not updated after unmount");
    return 0;
}

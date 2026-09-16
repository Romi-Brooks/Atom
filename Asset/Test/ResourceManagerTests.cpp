/**
 * @file           : ResourceManagerTests.cpp
 * @brief          : ResourceId, cache dedupe, type check, and recycle tests.
**/

#include <Asset/ResourceManager.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
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

class StringConfigLoader final : public atom::asset::TypedResourceLoader<std::string> {
    public:
        [[nodiscard]] auto Kind() const -> atom::asset::AssetKind override {
            return atom::asset::AssetKind::Config;
        }

    protected:
        auto LoadTyped(const atom::fs::IFileSystem& filesystem, const atom::asset::ResourceId& id,
                       std::shared_ptr<std::string>& output) -> atom::asset::AssetResult override {
            std::unique_ptr<atom::fs::IFile> file{};
            const atom::fs::Result opened = filesystem.OpenRead(id.Path(), file);
            if (opened != atom::fs::Result::Success)
                return atom::asset::MapFsResult(opened);
            std::vector<std::byte> data{};
            if (atom::fs::ReadAll(*file, data) != atom::fs::Result::Success)
                return atom::asset::AssetResult::LoadFailed;
            output = std::make_shared<std::string>(reinterpret_cast<const char*>(data.data()), data.size());
            return atom::asset::AssetResult::Success;
        }
};

class IntScriptLoader final : public atom::asset::TypedResourceLoader<int> {
    public:
        [[nodiscard]] auto Kind() const -> atom::asset::AssetKind override {
            return atom::asset::AssetKind::Script;
        }

    protected:
        auto LoadTyped(const atom::fs::IFileSystem& /*filesystem*/, const atom::asset::ResourceId& /*id*/,
                       std::shared_ptr<int>& output) -> atom::asset::AssetResult override {
            output = std::make_shared<int>(42);
            return atom::asset::AssetResult::Success;
        }
};

} // namespace

auto main() -> int {
    std::unique_ptr<atom::fs::MemoryFileSystem> memory_owned{};
    if (atom::fs::MemoryFileSystem::Create("res", memory_owned) != atom::fs::Result::Success || !memory_owned)
        return Fail("Could not create memory filesystem");
    auto memory = std::shared_ptr<atom::fs::MemoryFileSystem>{std::move(memory_owned)};

    atom::fs::AssetPath config_path{};
    atom::fs::AssetPath missing_path{};
    if (!atom::fs::AssetPath::TryParse("res://config/default.txt", config_path) ||
        !atom::fs::AssetPath::TryParse("res://config/missing.txt", missing_path)) {
        return Fail("Could not parse asset paths");
    }
    if (memory->WriteFile(config_path, Bytes("hello-atom")) != atom::fs::Result::Success)
        return Fail("Could not seed memory file");

    atom::asset::ResourceId config_id{};
    atom::asset::ResourceId missing_id{};
    atom::asset::ResourceId bad_id{};
    if (!atom::asset::ResourceId::TryCreate(config_path, atom::asset::AssetKind::Config, "", config_id))
        return Fail("Could not create config id");
    if (!atom::asset::ResourceId::TryCreate(missing_path, atom::asset::AssetKind::Config, "", missing_id))
        return Fail("Could not create missing id");
    if (atom::asset::ResourceId::TryCreate(config_path, atom::asset::AssetKind::Unknown, "", bad_id))
        return Fail("Unknown kind was accepted");

    atom::asset::ResourceId config_hd{};
    if (!atom::asset::ResourceId::TryCreate(config_path, atom::asset::AssetKind::Config, "hd", config_hd))
        return Fail("Could not create hd variant id");
    if (config_id == config_hd || config_id.Key() == config_hd.Key())
        return Fail("Variant must change identity");
    if (!(config_id == config_id))
        return Fail("Identity must equal itself");

    atom::asset::ResourceManager manager{*memory};
    if (manager.HasLoader(atom::asset::AssetKind::Config))
        return Fail("Loader present before registration");

    if (manager.RegisterLoader(std::make_shared<StringConfigLoader>()) != atom::asset::AssetResult::Success)
        return Fail("Could not register config loader");
    if (manager.RegisterLoader(std::make_shared<IntScriptLoader>()) != atom::asset::AssetResult::Success)
        return Fail("Could not register script loader");
    if (!manager.HasLoader(atom::asset::AssetKind::Config) || !manager.HasLoader(atom::asset::AssetKind::Script))
        return Fail("Loader registration not visible");

    // NoLoader for unregistered kind.
    atom::asset::ResourceId texture_id{};
    if (!atom::asset::ResourceId::TryCreate(config_path, atom::asset::AssetKind::Texture, "", texture_id))
        return Fail("Could not create texture id");
    if (manager.Acquire<std::string>(texture_id))
        return Fail("Acquire succeeded without a loader");

    // Missing file.
    if (manager.Acquire<std::string>(missing_id))
        return Fail("Acquire succeeded for a missing file");

    // Dedupe: same id, same instance.
    auto first = manager.Acquire<std::string>(config_id);
    auto second = manager.Acquire<std::string>(config_id);
    if (!first || !second)
        return Fail("Acquire failed for valid config");
    if (first.Get() != second.Get())
        return Fail("Cache did not dedupe by ResourceId");
    if (*first != "hello-atom")
        return Fail("Incorrect resource contents");
    if (manager.LiveCount() != 1)
        return Fail("LiveCount should be 1 while handles are held");

    // Type mismatch: config id requested as int must fail.
    if (manager.Acquire<int>(config_id))
        return Fail("Type mismatch was not rejected");

    // Variant is a different cache entry (will fail to load as config content
    // still exists under the same path — loader is path-based so it succeeds
    // but must be a distinct instance).
    auto hd = manager.Acquire<std::string>(config_hd);
    if (!hd)
        return Fail("Acquire failed for hd variant");
    if (hd.Get() == first.Get())
        return Fail("Variant shared the default instance");

    // Evict: live handles keep the resource; new acquire reloads.
    if (!manager.Evict(config_id))
        return Fail("Evict failed");
    if (manager.CacheCapacity() != 1)
        return Fail("Evict did not shrink the cache");
    auto third = manager.Acquire<std::string>(config_id);
    if (!third)
        return Fail("Acquire after evict failed");
    if (third.Get() == first.Get())
        return Fail("Evict did not force a reload");

    // Recycle callback fires on last handle release.
    first = {};
    second = {};
    third = {};
    hd = {};
    manager.EvictAll();

    int recycled = 0;
    atom::asset::ResourceId recycled_id{};
    manager.SetRecycleCallback(
        [&](const atom::asset::ResourceId& id, const uint32_t generation, std::shared_ptr<void> resource) {
            ++recycled;
            recycled_id = id;
            if (generation == 0 || !resource)
                std::cerr << "Unexpected recycle payload\n";
        });

    {
        auto temp = manager.Acquire<std::string>(config_id);
        if (!temp)
            return Fail("Acquire for recycle test failed");
        if (manager.LiveCount() != 1)
            return Fail("LiveCount should be 1 while temp is held");
    }
    if (recycled != 1)
        return Fail("Recycle callback was not invoked once");
    if (!(recycled_id == config_id))
        return Fail("Recycle callback id mismatch");

    // Script loader path.
    atom::fs::AssetPath script_path{};
    if (!atom::fs::AssetPath::TryParse("res://scripts/main.lua", script_path))
        return Fail("Could not parse script path");
    atom::asset::ResourceId script_id{};
    if (!atom::asset::ResourceId::TryCreate(script_path, atom::asset::AssetKind::Script, "", script_id))
        return Fail("Could not create script id");
    auto script = manager.Acquire<int>(script_id);
    if (!script || *script != 42)
        return Fail("Script loader did not produce expected value");

    // Vfs as the filesystem for the manager.
    atom::fs::Vfs vfs{};
    if (vfs.Mount("res", 100, memory) != atom::fs::Result::Success)
        return Fail("Could not mount vfs");
    atom::asset::ResourceManager vfs_manager{vfs};
    if (vfs_manager.RegisterLoader(std::make_shared<StringConfigLoader>()) != atom::asset::AssetResult::Success)
        return Fail("Could not register loader on vfs manager");
    auto via_vfs = vfs_manager.Acquire<std::string>(config_id);
    if (!via_vfs || *via_vfs != "hello-atom")
        return Fail("Acquire through Vfs failed");

    return 0;
}

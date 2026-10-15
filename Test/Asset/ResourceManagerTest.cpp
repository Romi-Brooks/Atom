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
        [[nodiscard]] auto LoadTyped(const atom::fs::IFileSystem& filesystem, const atom::asset::ResourceId& id,
                                     std::shared_ptr<std::string>& output) -> atom::Result<void> override {
            std::unique_ptr<atom::fs::IFile> file{};
            const atom::fs::Result opened = filesystem.OpenRead(id.Path(), file);
            if (opened != atom::fs::Result::Success)
                return std::unexpected<atom::Error>{atom::fs::ToError(opened, "OpenRead", id.Path().String())};
            std::vector<std::byte> data{};
            const atom::fs::Result read = atom::fs::ReadAll(*file, data);
            if (read != atom::fs::Result::Success)
                return std::unexpected<atom::Error>{atom::fs::ToError(read, "ReadAll", id.Path().String())};
            output = std::make_shared<std::string>(reinterpret_cast<const char*>(data.data()), data.size());
            return atom::Ok();
        }
};

class IntScriptLoader final : public atom::asset::TypedResourceLoader<int> {
    public:
        [[nodiscard]] auto Kind() const -> atom::asset::AssetKind override {
            return atom::asset::AssetKind::Script;
        }

    protected:
        [[nodiscard]] auto LoadTyped(const atom::fs::IFileSystem& /*filesystem*/, const atom::asset::ResourceId& /*id*/,
                                     std::shared_ptr<int>& output) -> atom::Result<void> override {
            output = std::make_shared<int>(42);
            return atom::Ok();
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

    if (!manager.RegisterLoader(std::make_shared<StringConfigLoader>()))
        return Fail("Could not register config loader");
    if (!manager.RegisterLoader(std::make_shared<IntScriptLoader>()))
        return Fail("Could not register script loader");
    if (!manager.HasLoader(atom::asset::AssetKind::Config) || !manager.HasLoader(atom::asset::AssetKind::Script))
        return Fail("Loader registration not visible");

    // A rejected registration must name what was wrong, not just fail.
    if (const auto rejected = manager.RegisterLoader(nullptr); rejected.has_value())
        return Fail("Null loader registration succeeded");
    else if (rejected.error().Code() != atom::ErrorCode::AssetInvalidId)
        return Fail("Null loader registration reported the wrong code");

    // NoLoader for unregistered kind.
    atom::asset::ResourceId texture_id{};
    if (!atom::asset::ResourceId::TryCreate(config_path, atom::asset::AssetKind::Texture, "", texture_id))
        return Fail("Could not create texture id");
    if (manager.Acquire<std::string>(texture_id))
        return Fail("Acquire succeeded without a loader");

    // TryAcquire is the diagnosable form: it must say *why* the acquire failed,
    // which is what Acquire() alone could never report (CORE-004).
    {
        const auto no_loader = manager.TryAcquire<std::string>(texture_id);
        if (no_loader.has_value())
            return Fail("TryAcquire succeeded without a loader");
        if (no_loader.error().Code() != atom::ErrorCode::AssetNoLoader)
            return Fail("TryAcquire reported the wrong code for a missing loader");
        if (no_loader.error().Resource() != texture_id.Path().String())
            return Fail("TryAcquire error did not carry the resource path");
        if (no_loader.error().Operation() != "AcquireRecord")
            return Fail("TryAcquire error did not carry the operation");

        const auto missing = manager.TryAcquire<std::string>(missing_id);
        if (missing.has_value())
            return Fail("TryAcquire succeeded for a missing file");
        if (missing.error().Code() != atom::ErrorCode::FsNotFound)
            return Fail("TryAcquire did not forward the loader's own reason");
        // The lowest layer that knew what it was doing names the operation, and
        // the re-reporting layers do not overwrite it: "OpenRead failed" is more
        // useful than "AcquireRecord failed".
        if (missing.error().Operation() != "OpenRead")
            return Fail("Forwarded error lost the innermost operation");
    }

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

    // Type mismatch: config id requested as int must fail, and TryAcquire must
    // be able to say that the type was the problem.
    if (manager.Acquire<int>(config_id))
        return Fail("Type mismatch was not rejected");
    {
        const auto mismatched = manager.TryAcquire<int>(config_id);
        if (mismatched.has_value())
            return Fail("TryAcquire accepted a type mismatch");
        if (mismatched.error().Code() != atom::ErrorCode::AssetTypeMismatch)
            return Fail("TryAcquire reported the wrong code for a type mismatch");
    }

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
    if (!vfs_manager.RegisterLoader(std::make_shared<StringConfigLoader>()))
        return Fail("Could not register loader on vfs manager");
    auto via_vfs = vfs_manager.Acquire<std::string>(config_id);
    if (!via_vfs || *via_vfs != "hello-atom")
        return Fail("Acquire through Vfs failed");

    return 0;
}

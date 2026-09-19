#include <Asset/ResourceManager.hpp>
#include <Asset/ScriptSourceLoader.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <memory>
#include <span>
#include <string>
#include <typeindex>
#include <vector>

namespace {

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

} // namespace

auto main() -> int {
    using namespace atom::asset;

    std::unique_ptr<atom::fs::MemoryFileSystem> owned{};
    ATOM_CHECK(atom::fs::MemoryFileSystem::Create("res", owned) == atom::fs::Result::Success && owned);
    auto memory = std::shared_ptr<atom::fs::MemoryFileSystem>{std::move(owned)};

    atom::fs::AssetPath script_path{};
    atom::fs::AssetPath missing_path{};
    atom::fs::AssetPath empty_path{};
    atom::fs::AssetPath config_path{};
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://scripts/main.lua", script_path));
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://scripts/missing.lua", missing_path));
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://scripts/empty.lua", empty_path));
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://config/settings.toml", config_path));
    ATOM_CHECK(memory->WriteFile(script_path, Bytes("return 42")) == atom::fs::Result::Success);
    ATOM_CHECK(memory->WriteFile(empty_path, Bytes("")) == atom::fs::Result::Success);

    ResourceId script_id{};
    ResourceId missing_id{};
    ResourceId empty_id{};
    ResourceId config_id{};
    ATOM_CHECK(ResourceId::TryCreate(script_path, AssetKind::Script, "", script_id));
    ATOM_CHECK(ResourceId::TryCreate(missing_path, AssetKind::Script, "", missing_id));
    ATOM_CHECK(ResourceId::TryCreate(empty_path, AssetKind::Script, "", empty_id));
    ATOM_CHECK(ResourceId::TryCreate(config_path, AssetKind::Config, "", config_id));

    atom::ScriptSourceLoader loader{};
    ATOM_CHECK(loader.Kind() == AssetKind::Script);
    ATOM_CHECK(loader.ResourceType() == typeid(std::string));

    ResourceManager manager{*memory};
    ATOM_CHECK(manager.RegisterLoader(std::make_shared<atom::ScriptSourceLoader>()) == AssetResult::Success);

    auto source = manager.Acquire<std::string>(script_id);
    ATOM_CHECK(source);
    ATOM_CHECK(*source == "return 42");

    auto again = manager.Acquire<std::string>(script_id);
    ATOM_CHECK(again.Get() == source.Get());

    auto missing = manager.Acquire<std::string>(missing_id);
    ATOM_CHECK(!missing);

    auto empty = manager.Acquire<std::string>(empty_id);
    ATOM_CHECK(empty);
    ATOM_CHECK(empty->empty());

    // Wrong kind must not go through the script loader.
    auto wrong_kind = manager.Acquire<std::string>(config_id);
    ATOM_CHECK(!wrong_kind);
    return 0;
}

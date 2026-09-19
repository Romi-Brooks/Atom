#include <Asset/AssetKind.hpp>
#include <Asset/ResourceId.hpp>
#include <Filesystem/AssetPath.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::asset;

    ATOM_CHECK(ToString(AssetKind::Texture) == "texture");
    ATOM_CHECK(ToString(AssetKind::Unknown) == "unknown");
    ATOM_CHECK(AssetKindFromString("audio") == AssetKind::Audio);
    ATOM_CHECK(AssetKindFromString("nope") == AssetKind::Unknown);
    ATOM_CHECK(AssetKindFromString("script") == AssetKind::Script);

    for (const auto kind : {AssetKind::Binary, AssetKind::Texture, AssetKind::Audio, AssetKind::Shader,
                            AssetKind::Config, AssetKind::Script, AssetKind::Font, AssetKind::Model,
                            AssetKind::Scene, AssetKind::Video}) {
        ATOM_CHECK(AssetKindFromString(ToString(kind)) == kind);
    }

    atom::fs::AssetPath path{};
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://textures/ui.png", path));

    ResourceId id{};
    ResourceIdError error{};
    ATOM_CHECK(ResourceId::TryCreate(path, AssetKind::Texture, "", id, &error));
    ATOM_CHECK(error == ResourceIdError::None);
    ATOM_CHECK(id.IsValid());
    ATOM_CHECK(id.Kind() == AssetKind::Texture);
    ATOM_CHECK(id.Variant().empty());
    ATOM_CHECK(!id.Key().empty());

    ResourceId unknown{};
    ATOM_CHECK(!ResourceId::TryCreate(path, AssetKind::Unknown, "", unknown, &error));
    ATOM_CHECK(error == ResourceIdError::InvalidKind);

    ResourceId bad_variant{};
    ATOM_CHECK(!ResourceId::TryCreate(path, AssetKind::Texture, "HD!", bad_variant, &error));
    ATOM_CHECK(error == ResourceIdError::InvalidVariant);

    ResourceId invalid_path{};
    atom::fs::AssetPath bad_path{};
    ATOM_CHECK(!ResourceId::TryCreate(bad_path, AssetKind::Texture, "", invalid_path, &error));
    ATOM_CHECK(error == ResourceIdError::InvalidPath);

    ResourceId hd{};
    ATOM_CHECK(ResourceId::TryCreate(path, AssetKind::Texture, "hd", hd));
    ATOM_CHECK(!(id == hd));
    ATOM_CHECK(id.Key() != hd.Key());

    ResourceId same{};
    ATOM_CHECK(ResourceId::TryCreate(path, AssetKind::Texture, "", same));
    ATOM_CHECK(id == same);
    ATOM_CHECK(id.Key() == same.Key());

    ResourceId other_kind{};
    ATOM_CHECK(ResourceId::TryCreate(path, AssetKind::Config, "", other_kind));
    ATOM_CHECK(!(id == other_kind));
    return 0;
}

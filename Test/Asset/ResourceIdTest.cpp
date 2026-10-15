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

    // Create() reports the reason a rejected identity was rejected; the
    // convenience form only reports that it was.
    const auto created = ResourceId::Create(path, AssetKind::Texture, "");
    ATOM_CHECK(created.has_value());
    const auto& id = *created;
    ATOM_CHECK(id.IsValid());
    ATOM_CHECK(id.Kind() == AssetKind::Texture);
    ATOM_CHECK(id.Variant().empty());
    ATOM_CHECK(!id.Key().empty());

    const auto unknown = ResourceId::Create(path, AssetKind::Unknown, "");
    ATOM_CHECK(!unknown.has_value());
    ATOM_CHECK(unknown.error().Code() == atom::ErrorCode::AssetInvalidId);
    ATOM_CHECK(unknown.error().Detail() == ResourceIdErrorName(ResourceIdError::InvalidKind));
    ATOM_CHECK(unknown.error().GetModule() == atom::Module::Asset);

    const auto bad_variant = ResourceId::Create(path, AssetKind::Texture, "HD!");
    ATOM_CHECK(!bad_variant.has_value());
    ATOM_CHECK(bad_variant.error().Detail() == ResourceIdErrorName(ResourceIdError::InvalidVariant));
    // The resource that was rejected is part of the diagnostic, so a log line
    // can be traced back to the identity that asked for it.
    ATOM_CHECK(bad_variant.error().Resource() == "res://textures/ui.png");

    const auto invalid_path = ResourceId::Create(atom::fs::AssetPath{}, AssetKind::Texture, "");
    ATOM_CHECK(!invalid_path.has_value());
    ATOM_CHECK(invalid_path.error().Detail() == ResourceIdErrorName(ResourceIdError::InvalidPath));

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

/**
 * @file           : ResourceId.cpp
 * @brief          : ResourceId construction and canonical key.
**/

#include "ResourceId.hpp"

namespace atom::asset {
namespace {

[[nodiscard]] auto IsVariantAllowed(const std::string_view variant) -> bool {
    if (variant.empty())
        return true;
    if (variant.size() > 64)
        return false;
    for (const char value : variant) {
        const bool ok = (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '-' ||
                        value == '_';
        if (!ok)
            return false;
    }
    return true;
}

[[nodiscard]] auto BuildKey(const fs::AssetPath& path, const AssetKind kind, const std::string_view variant)
    -> std::string {
    std::string key{};
    key.reserve(8 + variant.size() + path.String().size());
    key.append(ToString(kind));
    key.push_back('\x1f');
    key.append(variant);
    key.push_back('\x1f');
    key.append(path.String());
    return key;
}

} // namespace

auto ToError(const ResourceIdError error, const std::string_view operation) -> Error {
    ErrorCode code = ErrorCode::AssetInvalidId;
    switch (error) {
    case ResourceIdError::None:
        code = ErrorCode::Internal;
        break;
    case ResourceIdError::InvalidPath:
    case ResourceIdError::InvalidKind:
    case ResourceIdError::InvalidVariant:
        code = ErrorCode::AssetInvalidId;
        break;
    }
    return Error{code, Module::Asset, std::string{operation}}.WithDetail(
        std::string{ResourceIdErrorName(error)});
}

auto ResourceId::Create(const fs::AssetPath& path, const AssetKind kind, const std::string_view variant)
    -> Result<ResourceId> {
    if (!path.IsValid()) {
        return std::unexpected<Error>{
            ToError(ResourceIdError::InvalidPath, "ResourceId::Create").WithResource(path.String())};
    }
    if (kind == AssetKind::Unknown)
        return std::unexpected<Error>{ToError(ResourceIdError::InvalidKind, "ResourceId::Create")};
    if (!IsVariantAllowed(variant)) {
        return std::unexpected<Error>{ToError(ResourceIdError::InvalidVariant, "ResourceId::Create")
                                          .WithResource(path.String())
                                          .WithDetail(std::string{variant})};
    }

    ResourceId id{};
    id.path_ = path;
    id.kind_ = kind;
    id.variant_ = variant;
    id.key_ = BuildKey(path, kind, variant);
    return id;
}

auto ResourceId::TryCreate(const fs::AssetPath& path, const AssetKind kind, const std::string_view variant,
                           ResourceId& output) -> bool {
    auto created = Create(path, kind, variant);
    if (!created.has_value())
        return false;
    output = std::move(*created);
    return true;
}

auto ResourceId::Path() const -> const fs::AssetPath& { return path_; }

auto ResourceId::Kind() const -> AssetKind { return kind_; }

auto ResourceId::Variant() const -> std::string_view { return variant_; }

auto ResourceId::IsValid() const -> bool { return path_.IsValid() && kind_ != AssetKind::Unknown; }

auto ResourceId::Key() const -> std::string_view { return key_; }

auto ResourceId::operator==(const ResourceId& other) const -> bool {
    return kind_ == other.kind_ && variant_ == other.variant_ && path_.String() == other.path_.String();
}

} // namespace atom::asset

/**
 * @file           : ResourceId.cpp
 * @brief          : ResourceId construction and canonical key.
**/

#include "ResourceId.hpp"

namespace atom::asset {
namespace {

void SetError(ResourceIdError* error, const ResourceIdError value) {
    if (error)
        *error = value;
}

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

auto ResourceId::TryCreate(const fs::AssetPath& path, const AssetKind kind, const std::string_view variant,
                           ResourceId& output, ResourceIdError* error) -> bool {
    SetError(error, ResourceIdError::None);
    output = ResourceId{};
    if (!path.IsValid()) {
        SetError(error, ResourceIdError::InvalidPath);
        return false;
    }
    if (kind == AssetKind::Unknown) {
        SetError(error, ResourceIdError::InvalidKind);
        return false;
    }
    if (!IsVariantAllowed(variant)) {
        SetError(error, ResourceIdError::InvalidVariant);
        return false;
    }

    output.path_ = path;
    output.kind_ = kind;
    output.variant_ = variant;
    output.key_ = BuildKey(path, kind, variant);
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

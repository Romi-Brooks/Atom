/**
 * @file           : ResourceId.hpp
 * @brief          : Stable identity for a loadable runtime resource.
**/

#ifndef ATOM_ASSET_RESOURCE_ID_HPP
#define ATOM_ASSET_RESOURCE_ID_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include <Asset/AssetKind.hpp>
#include <Error/Error.hpp>
#include <Filesystem/AssetPath.hpp>

namespace atom::asset {

// Why an identity was rejected. Kept next to the validation rules; Create()
// converts it to the engine's unified Error (CORE-004).
enum class ResourceIdError : uint8_t {
        None,
        InvalidPath,
        InvalidKind,
        InvalidVariant,
};

[[nodiscard]] auto ToError(ResourceIdError error, std::string_view operation = {}) -> Error;

[[nodiscard]] constexpr auto ResourceIdErrorName(ResourceIdError error) -> std::string_view {
    switch (error) {
        case ResourceIdError::None:
            return "None";
        case ResourceIdError::InvalidPath:
            return "InvalidPath";
        case ResourceIdError::InvalidKind:
            return "InvalidKind";
        case ResourceIdError::InvalidVariant:
            return "InvalidVariant";
    }
    return "UnknownResourceIdError";
}

// Identity is (path, kind, variant). Two ids are equal only when all three
// match. The empty variant is the default platform/quality variant.
class ResourceId final {
    public:
        // The one way to build an identity. The failure names the offending
        // field, and carries the path when there is a valid one to report.
        [[nodiscard]] static auto Create(const fs::AssetPath& path, AssetKind kind, std::string_view variant)
            -> Result<ResourceId>;

        // Convenience form for call sites that only branch on validity. It
        // forwards to Create(); prefer Create() when the reason matters.
        [[nodiscard]] static auto TryCreate(const fs::AssetPath& path, AssetKind kind, std::string_view variant,
                                            ResourceId& output) -> bool;

        [[nodiscard]] auto Path() const -> const fs::AssetPath&;
        [[nodiscard]] auto Kind() const -> AssetKind;
        [[nodiscard]] auto Variant() const -> std::string_view;
        [[nodiscard]] auto IsValid() const -> bool;

        // Canonical cache key. Stable for a given (path, kind, variant).
        [[nodiscard]] auto Key() const -> std::string_view;

        auto operator==(const ResourceId& other) const -> bool;

    private:
        fs::AssetPath path_{};
        AssetKind kind_ = AssetKind::Unknown;
        std::string variant_{};
        std::string key_{};
};

} // namespace atom::asset

#endif // ATOM_ASSET_RESOURCE_ID_HPP

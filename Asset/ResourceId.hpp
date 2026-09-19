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
#include <Filesystem/AssetPath.hpp>

namespace atom::asset {

enum class ResourceIdError : uint8_t {
        None,
        InvalidPath,
        InvalidKind,
        InvalidVariant,
};

// Identity is (path, kind, variant). Two ids are equal only when all three
// match. The empty variant is the default platform/quality variant.
class ResourceId final {
    public:
        [[nodiscard]] static auto TryCreate(const fs::AssetPath& path, AssetKind kind, std::string_view variant,
                                            ResourceId& output, ResourceIdError* error = nullptr) -> bool;

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

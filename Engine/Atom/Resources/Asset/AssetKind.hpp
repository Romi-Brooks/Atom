/**
 * @file           : AssetKind.hpp
 * @brief          : Runtime asset classification independent of file extension.
**/

#ifndef ATOM_ASSET_ASSET_KIND_HPP
#define ATOM_ASSET_ASSET_KIND_HPP

#include <cstdint>
#include <string_view>

#include <Atom/Build/CoreExport.hpp>

namespace atom::asset {

enum class AssetKind : uint8_t {
    Unknown,
    Binary,
    Texture,
    Audio,
    Shader,
    Config,
    Script,
    Font,
    Model,
    Scene,
    Video,
};

[[nodiscard]] ATOM_CORE_API auto ToString(AssetKind kind) -> std::string_view;
[[nodiscard]] ATOM_CORE_API auto AssetKindFromString(std::string_view value) -> AssetKind;

} // namespace atom::asset

#endif // ATOM_ASSET_ASSET_KIND_HPP

/**
 * @file           : AssetKind.cpp
 * @brief          : AssetKind string conversion.
**/

#include "AssetKind.hpp"

namespace atom::asset {

auto ToString(const AssetKind kind) -> std::string_view {
    switch (kind) {
    case AssetKind::Binary:
        return "binary";
    case AssetKind::Texture:
        return "texture";
    case AssetKind::Audio:
        return "audio";
    case AssetKind::Shader:
        return "shader";
    case AssetKind::Config:
        return "config";
    case AssetKind::Script:
        return "script";
    case AssetKind::Font:
        return "font";
    case AssetKind::Model:
        return "model";
    case AssetKind::Scene:
        return "scene";
    case AssetKind::Video:
        return "video";
    case AssetKind::Unknown:
        break;
    }
    return "unknown";
}

auto AssetKindFromString(const std::string_view value) -> AssetKind {
    if (value == "binary")
        return AssetKind::Binary;
    if (value == "texture")
        return AssetKind::Texture;
    if (value == "audio")
        return AssetKind::Audio;
    if (value == "shader")
        return AssetKind::Shader;
    if (value == "config")
        return AssetKind::Config;
    if (value == "script")
        return AssetKind::Script;
    if (value == "font")
        return AssetKind::Font;
    if (value == "model")
        return AssetKind::Model;
    if (value == "scene")
        return AssetKind::Scene;
    if (value == "video")
        return AssetKind::Video;
    return AssetKind::Unknown;
}

} // namespace atom::asset

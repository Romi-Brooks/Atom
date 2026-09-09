#include <Filesystem/AssetPath.hpp>

#include <array>
#include <iostream>

namespace {

auto ExpectValid(const std::string_view value) -> bool {
    atom::fs::AssetPath path{};
    atom::fs::PathError error{};
    if (!atom::fs::AssetPath::TryParse(value, path, &error)) {
        std::cerr << "Expected valid path: " << value << '\n';
        return false;
    }
    return path.String() == value;
}

auto ExpectInvalid(const std::string_view value) -> bool {
    atom::fs::AssetPath path{};
    return !atom::fs::AssetPath::TryParse(value, path);
}

} // namespace

auto main() -> int {
    constexpr std::array valid{
        "res://",
        "res://textures/ui/button.png",
        "engine://shaders/spirv/sprite.spv",
        "mod-01://config/default.toml",
    };
    constexpr std::array invalid{
        "",
        "textures/ui/button.png",
        "RES://textures/ui/button.png",
        "res://textures//button.png",
        "res://textures/../button.png",
        "res://textures\\button.png",
        "res://textures/./button.png",
        "res://textures/",
        "res://textures:button.png",
    };
    for (const auto value : valid) {
        if (!ExpectValid(value))
            return 1;
    }
    for (const auto value : invalid) {
        if (!ExpectInvalid(value))
            return 1;
    }
    return 0;
}

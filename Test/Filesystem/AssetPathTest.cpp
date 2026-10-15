#include <Filesystem/AssetPath.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace {

using atom::ErrorCode;
using atom::fs::AssetPath;
using atom::fs::PathError;

auto ExpectValid(const std::string_view value) -> bool {
    const auto path = AssetPath::Parse(value);
    if (!path.has_value()) {
        std::cerr << "Expected valid path: " << value << " (" << path.error().Message() << ")\n";
        return false;
    }
    return path->String() == value;
}

// Asserts both that the value is rejected and that the reported code names the
// rule that rejected it - the reason is the point of the unified error model,
// so a bare "it failed" is no longer enough.
auto ExpectInvalid(const std::string_view value, const ErrorCode expected, const PathError path_error) -> bool {
    const auto path = AssetPath::Parse(value);
    if (path.has_value()) {
        std::cerr << "Expected invalid path: " << value << '\n';
        return false;
    }
    if (path.error().Code() != expected) {
        std::cerr << "Wrong code for '" << value << "': got "
                  << atom::ErrorCodeName(path.error().Code()) << ", want " << atom::ErrorCodeName(expected) << '\n';
        return false;
    }
    if (path.error().Detail() != atom::fs::PathErrorName(path_error)) {
        std::cerr << "Wrong path error detail for '" << value << "': got " << path.error().Detail() << '\n';
        return false;
    }
    return true;
}

} // namespace

auto main() -> int {
    constexpr std::array valid{
        "res://",
        "res://textures/ui/button.png",
        "engine://shaders/spirv/sprite.spv",
        "mod-01://config/default.toml",
    };
    for (const auto value : valid) {
        if (!ExpectValid(value))
            return 1;
    }

    // Each rejected input must name the rule it broke.
    if (!ExpectInvalid("", ErrorCode::FsInvalidPath, PathError::Empty))
        return 1;
    if (!ExpectInvalid("textures/ui/button.png", ErrorCode::FsMissingMount, PathError::MissingMount))
        return 1;
    if (!ExpectInvalid("RES://textures/ui/button.png", ErrorCode::FsInvalidMount, PathError::InvalidMount))
        return 1;
    if (!ExpectInvalid("res://textures//button.png", ErrorCode::FsInvalidSeparator, PathError::InvalidSeparator))
        return 1;
    if (!ExpectInvalid("res://textures\\button.png", ErrorCode::FsInvalidSeparator, PathError::InvalidSeparator))
        return 1;
    if (!ExpectInvalid("res://textures/../button.png", ErrorCode::FsInvalidPath, PathError::InvalidSegment))
        return 1;
    if (!ExpectInvalid("res://textures/./button.png", ErrorCode::FsInvalidPath, PathError::InvalidSegment))
        return 1;
    if (!ExpectInvalid("res://textures/", ErrorCode::FsInvalidPath, PathError::InvalidSegment))
        return 1;
    if (!ExpectInvalid("res://textures:button.png", ErrorCode::FsInvalidPath, PathError::InvalidSegment))
        return 1;
    // 0xC3 followed by '(' is a truncated two-byte sequence: invalid UTF-8.
    if (!ExpectInvalid("res://textures/\xC3" "(.png", ErrorCode::FsInvalidUtf8, PathError::InvalidUtf8))
        return 1;

    // The convenience form stays a thin wrapper over Parse(): it must reject
    // exactly what Parse() rejects and leave the output untouched.
    {
        AssetPath output{};
        if (AssetPath::TryParse("res://textures//ui.png", output)) {
            std::cerr << "TryParse accepted an invalid path\n";
            return 1;
        }
        if (output.IsValid()) {
            std::cerr << "TryParse wrote to the output on failure\n";
            return 1;
        }
        if (!AssetPath::TryParse("res://textures/ui.png", output)) {
            std::cerr << "TryParse rejected a valid path\n";
            return 1;
        }
        if (output.String() != "res://textures/ui.png")
            return 1;
    }

    return 0;
}

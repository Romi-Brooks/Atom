/**
 * @file           : AssetPath.hpp
 * @brief          : Canonical, platform-independent virtual resource path.
 * @attention      : Asset paths are UTF-8 and never contain native paths.
**/

#ifndef ATOM_FILESYSTEM_ASSET_PATH_HPP
#define ATOM_FILESYSTEM_ASSET_PATH_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include <Error/Error.hpp>

namespace atom::fs {

// Why a path was rejected. Kept as a code enum next to the parser so the
// validation rules and their names stay in one file; Parse() converts it to the
// engine's unified Error (CORE-004).
enum class PathError : uint8_t {
        None,
        Empty,
        InvalidUtf8,
        MissingMount,
        InvalidMount,
        InvalidSeparator,
        InvalidSegment,
};

[[nodiscard]] auto ToError(PathError error, std::string_view operation = {}) -> Error;

[[nodiscard]] constexpr auto PathErrorName(PathError error) -> std::string_view {
    switch (error) {
        case PathError::None:
            return "None";
        case PathError::Empty:
            return "Empty";
        case PathError::InvalidUtf8:
            return "InvalidUtf8";
        case PathError::MissingMount:
            return "MissingMount";
        case PathError::InvalidMount:
            return "InvalidMount";
        case PathError::InvalidSeparator:
            return "InvalidSeparator";
        case PathError::InvalidSegment:
            return "InvalidSegment";
    }
    return "UnknownPathError";
}

// A canonical resource identifier such as "res://textures/ui/button.png".
// It deliberately does not model drive letters, backslashes, or relative
// traversal. Native file paths stay inside filesystem backend implementations.
class AssetPath final {
    public:
        // The one way to build an AssetPath from external text. Failure carries
        // the exact rule that rejected the input:
        //     auto path = AssetPath::Parse(text);
        //     if (!path) { LOG_ERROR(channel, path.error().Message()); return; }
        [[nodiscard]] static auto Parse(std::string_view value) -> Result<AssetPath>;

        // Convenience form for call sites that only branch on validity and have
        // no use for the reason. It is not a second implementation: it forwards
        // to Parse(). Prefer Parse() when the failure is worth reporting.
        [[nodiscard]] static auto TryParse(std::string_view value, AssetPath& output) -> bool;

        [[nodiscard]] auto String() const -> std::string_view;
        [[nodiscard]] auto Mount() const -> std::string_view;
        [[nodiscard]] auto Relative() const -> std::string_view;
        [[nodiscard]] auto IsValid() const -> bool;
        [[nodiscard]] auto IsRoot() const -> bool;

    private:
        std::string value_{};
        std::size_t mount_end_ = 0;
};

} // namespace atom::fs

#endif // ATOM_FILESYSTEM_ASSET_PATH_HPP

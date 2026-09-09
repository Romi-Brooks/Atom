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

namespace atom::fs {

enum class PathError : uint8_t {
        None,
        Empty,
        InvalidUtf8,
        MissingMount,
        InvalidMount,
        InvalidSeparator,
        InvalidSegment,
};

// A canonical resource identifier such as "res://textures/ui/button.png".
// It deliberately does not model drive letters, backslashes, or relative
// traversal. Native file paths stay inside filesystem backend implementations.
class AssetPath final {
    public:
        [[nodiscard]] static auto TryParse(std::string_view value, AssetPath& output,
                                           PathError* error = nullptr) -> bool;

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

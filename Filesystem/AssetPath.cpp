/**
 * @file           : AssetPath.cpp
 * @brief          : AssetPath validation and canonicalization.
**/

#include "AssetPath.hpp"

#include <Utilities/Utf8/Utf8.hpp>

namespace atom::fs {
namespace {

auto SetError(PathError* error, const PathError value) -> void {
    if (error)
        *error = value;
}

[[nodiscard]] auto IsMountStart(const char value) -> bool {
    return value >= 'a' && value <= 'z';
}

[[nodiscard]] auto IsMountCharacter(const char value) -> bool {
    return IsMountStart(value) || (value >= '0' && value <= '9') || value == '-' || value == '_';
}

} // namespace

auto AssetPath::TryParse(const std::string_view value, AssetPath& output, PathError* error) -> bool {
    SetError(error, PathError::None);
    if (value.empty()) {
        SetError(error, PathError::Empty);
        return false;
    }
    if (!atom::IsValidUtf8(std::string{value})) {
        SetError(error, PathError::InvalidUtf8);
        return false;
    }

    const std::size_t delimiter = value.find("://");
    if (delimiter == std::string_view::npos) {
        SetError(error, PathError::MissingMount);
        return false;
    }
    if (delimiter == 0 || !IsMountStart(value.front())) {
        SetError(error, PathError::InvalidMount);
        return false;
    }
    for (std::size_t index = 1; index < delimiter; ++index) {
        if (!IsMountCharacter(value[index])) {
            SetError(error, PathError::InvalidMount);
            return false;
        }
    }

    const std::string_view relative = value.substr(delimiter + 3);
    if (relative.find('\\') != std::string_view::npos || relative.find("//") != std::string_view::npos) {
        SetError(error, PathError::InvalidSeparator);
        return false;
    }
    if (!relative.empty() && relative.ends_with('/')) {
        SetError(error, PathError::InvalidSegment);
        return false;
    }
    std::size_t segment_start = 0;
    while (segment_start < relative.size()) {
        const std::size_t slash = relative.find('/', segment_start);
        const std::size_t segment_end = slash == std::string_view::npos ? relative.size() : slash;
        const std::string_view segment = relative.substr(segment_start, segment_end - segment_start);
        if (segment.empty() || segment == "." || segment == ".." || segment.find('\0') != std::string_view::npos ||
            segment.find_first_of(":*?\"<>|") != std::string_view::npos ||
            segment.find_first_of("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                                  "\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f") != std::string_view::npos) {
            SetError(error, PathError::InvalidSegment);
            return false;
        }
        segment_start = segment_end + 1;
    }

    output.value_ = value;
    output.mount_end_ = delimiter;
    return true;
}

auto AssetPath::String() const -> std::string_view { return value_; }

auto AssetPath::Mount() const -> std::string_view { return std::string_view{value_}.substr(0, mount_end_); }

auto AssetPath::Relative() const -> std::string_view {
    if (value_.empty())
        return {};
    return std::string_view{value_}.substr(mount_end_ + 3);
}

auto AssetPath::IsValid() const -> bool { return !value_.empty(); }

auto AssetPath::IsRoot() const -> bool { return IsValid() && Relative().empty(); }

} // namespace atom::fs

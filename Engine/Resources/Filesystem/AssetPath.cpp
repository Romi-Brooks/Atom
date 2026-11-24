/**
 * @file           : AssetPath.cpp
 * @brief          : AssetPath validation and canonicalization.
**/

#include "AssetPath.hpp"

#include <Utilities/Utf8/Utf8.hpp>

namespace atom::fs {
namespace {

[[nodiscard]] auto IsMountStart(const char value) -> bool {
    return value >= 'a' && value <= 'z';
}

[[nodiscard]] auto IsMountCharacter(const char value) -> bool {
    return IsMountStart(value) || (value >= '0' && value <= '9') || value == '-' || value == '_';
}

// Returns the first rule the value violates, or PathError::None when it is a
// valid asset path. Split out of Parse() so the validation order - which is
// part of the observable behaviour - lives in one readable sequence.
[[nodiscard]] auto Validate(const std::string_view value) -> PathError {
    if (value.empty())
        return PathError::Empty;
    if (!atom::IsValidUtf8(std::string{value}))
        return PathError::InvalidUtf8;

    const std::size_t delimiter = value.find("://");
    if (delimiter == std::string_view::npos)
        return PathError::MissingMount;
    if (delimiter == 0 || !IsMountStart(value.front()))
        return PathError::InvalidMount;
    for (std::size_t index = 1; index < delimiter; ++index) {
        if (!IsMountCharacter(value[index]))
            return PathError::InvalidMount;
    }

    const std::string_view relative = value.substr(delimiter + 3);
    if (relative.find('\\') != std::string_view::npos || relative.find("//") != std::string_view::npos)
        return PathError::InvalidSeparator;
    if (!relative.empty() && relative.ends_with('/'))
        return PathError::InvalidSegment;
    std::size_t segment_start = 0;
    while (segment_start < relative.size()) {
        const std::size_t slash = relative.find('/', segment_start);
        const std::size_t segment_end = slash == std::string_view::npos ? relative.size() : slash;
        const std::string_view segment = relative.substr(segment_start, segment_end - segment_start);
        if (segment.empty() || segment == "." || segment == ".." || segment.find('\0') != std::string_view::npos ||
            segment.find_first_of(":*?\"<>|") != std::string_view::npos ||
            segment.find_first_of("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                                  "\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f") != std::string_view::npos) {
            return PathError::InvalidSegment;
        }
        segment_start = segment_end + 1;
    }
    return PathError::None;
}

} // namespace

auto ToError(const PathError error, const std::string_view operation) -> Error {
    ErrorCode code = ErrorCode::FsInvalidPath;
    switch (error) {
    case PathError::None:
        code = ErrorCode::Internal;
        break;
    case PathError::Empty:
    case PathError::InvalidSegment:
        code = ErrorCode::FsInvalidPath;
        break;
    case PathError::InvalidUtf8:
        code = ErrorCode::FsInvalidUtf8;
        break;
    case PathError::MissingMount:
        code = ErrorCode::FsMissingMount;
        break;
    case PathError::InvalidMount:
        code = ErrorCode::FsInvalidMount;
        break;
    case PathError::InvalidSeparator:
        code = ErrorCode::FsInvalidSeparator;
        break;
    }
    return Error{code, Module::Fs, std::string{operation}}.WithDetail(std::string{PathErrorName(error)});
}

auto AssetPath::Parse(const std::string_view value) -> Result<AssetPath> {
    const PathError error = Validate(value);
    if (error != PathError::None)
        return std::unexpected<Error>{ToError(error, "AssetPath::Parse")};

    AssetPath path{};
    path.value_ = value;
    path.mount_end_ = value.find("://");
    return path;
}

auto AssetPath::TryParse(const std::string_view value, AssetPath& output) -> bool {
    auto parsed = Parse(value);
    if (!parsed.has_value())
        return false;
    output = std::move(*parsed);
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

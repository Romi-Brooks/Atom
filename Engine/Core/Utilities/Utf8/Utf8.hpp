/**
  * @file           : Utf8.hpp
  * @author         : Romi Brooks
  * @brief          : UTF-8 encoding conversion utilities shared by all Atom modules
  * @attention      : On Windows the CRT converts narrow paths with the ANSI code
  *                   page, which corrupts UTF-8 paths containing non-ASCII
  *                   characters (e.g. Chinese). Use PathFromUtf8/PathToUtf8 at
  *                   filesystem boundaries. On POSIX paths are natively UTF-8.
  *                   Conversion is delegated to the pinned utfcpp submodule.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_UTILITIES_UTF8_HPP
#define ATOM_UTILITIES_UTF8_HPP

#include <filesystem>
#include <string>

namespace atom {

// Convert UTF-8 text to the platform wchar_t representation. On Windows this
// is UTF-16; on POSIX it is normally UTF-32. Returns an empty string when the
// input contains invalid UTF-8.
[[nodiscard]] auto Utf8ToWide(const std::string& utf8) -> std::wstring;

// Convert platform wchar_t text to UTF-8. Returns an empty string when the
// input contains an invalid code point sequence.
[[nodiscard]] auto Utf8FromWide(const std::wstring& wide) -> std::string;

// Return a filesystem path's stable UTF-8 representation. On Windows this
// deliberately goes through path.wstring() so the CRT ANSI code page is never
// involved; on POSIX path.u8string() is already the native UTF-8 form.
[[nodiscard]] auto PathToUtf8(const std::filesystem::path& path) -> std::string;

// Construct a filesystem path from UTF-8 text without routing through the
// Windows ANSI code page.
[[nodiscard]] auto PathFromUtf8(const std::string& utf8) -> std::filesystem::path;

// Validate UTF-8 or replace malformed byte sequences with U+FFFD. These
// helpers keep third-party codec details out of higher-level Atom modules.
[[nodiscard]] auto IsValidUtf8(const std::string& utf8) -> bool;
[[nodiscard]] auto ReplaceInvalidUtf8(const std::string& utf8) -> std::string;

} // namespace atom


#endif // ATOM_UTILITIES_UTF8_HPP

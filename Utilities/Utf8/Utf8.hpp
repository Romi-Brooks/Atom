/**
  * @file           : Utf8.hpp
  * @author         : Romi Brooks
  * @brief          : UTF-8 encoding conversion utilities shared by all Atom modules
  * @attention      : On Windows the CRT converts narrow paths with the ANSI code
  *                   page, which corrupts UTF-8 paths containing non-ASCII
  *                   characters (e.g. Chinese). Use Utf8ToWide before opening
  *                   such files. On POSIX paths are natively UTF-8, so this
  *                   helper is only needed on Windows.
  *                   Conversion is delegated to the pinned utfcpp submodule.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_UTILITIES_UTF8_HPP
#define ATOM_UTILITIES_UTF8_HPP

#include <string>

#ifdef _WIN32

namespace atom {

// Convert a UTF-8 encoded narrow string to a UTF-16 wide string suitable for
// the wide (wchar_t) file APIs on Windows (std::ifstream::open, _wfopen, ...).
// Returns an empty string when the input is empty or contains invalid UTF-8.
[[nodiscard]] auto Utf8ToWide(const std::string& utf8) -> std::wstring;

} // namespace atom

#endif // _WIN32

#endif // ATOM_UTILITIES_UTF8_HPP

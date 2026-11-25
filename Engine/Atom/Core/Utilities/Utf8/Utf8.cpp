/**
  * @file           : Utf8.cpp
  * @author         : Romi Brooks
  * @brief          : UTF-8 encoding conversion utilities shared by all Atom modules
  * @attention      : Implemented on top of the pinned utfcpp submodule
  *                   (ThirdParty/utfcpp) instead of hand-rolled platform code.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Utf8.hpp"

#include <iterator>

#include <utf8.h>

namespace atom {

auto Utf8ToWide(const std::string& utf8) -> std::wstring {
    std::wstring wide;
    try {
        if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
            utf8::utf8to16(utf8.begin(), utf8.end(), std::back_inserter(wide));
        } else {
            utf8::utf8to32(utf8.begin(), utf8.end(), std::back_inserter(wide));
        }
    } catch (const utf8::exception&) {
        return {};
    }
    return wide;
}

auto Utf8FromWide(const std::wstring& wide) -> std::string {
    std::string utf8;
    try {
        if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
            utf8::utf16to8(wide.begin(), wide.end(), std::back_inserter(utf8));
        } else {
            utf8::utf32to8(wide.begin(), wide.end(), std::back_inserter(utf8));
        }
    } catch (const utf8::exception&) {
        return {};
    }
    return utf8;
}

auto PathToUtf8(const std::filesystem::path& path) -> std::string {
    try {
#ifdef _WIN32
        return Utf8FromWide(path.wstring());
#else
        const auto utf8 = path.u8string();
        return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
#endif
    } catch (const std::exception&) {
        return {};
    }
}

auto PathFromUtf8(const std::string& utf8) -> std::filesystem::path {
#ifdef _WIN32
    return std::filesystem::path{Utf8ToWide(utf8)};
#else
    return std::filesystem::path{utf8};
#endif
}

auto IsValidUtf8(const std::string& utf8) -> bool {
    try {
        return utf8::is_valid(utf8.begin(), utf8.end());
    } catch (const utf8::exception&) {
        return false;
    }
}

auto ReplaceInvalidUtf8(const std::string& utf8) -> std::string {
    try {
        std::string sanitized;
        utf8::replace_invalid(utf8.begin(), utf8.end(), std::back_inserter(sanitized));
        return sanitized;
    } catch (const utf8::exception&) {
        return {};
    }
}

} // namespace atom

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

#ifdef _WIN32

#include <iterator>

#include <utf8.h>

namespace atom {

auto Utf8ToWide(const std::string& utf8) -> std::wstring {
    std::wstring wide;
    try {
        utf8::utf8to16(utf8.begin(), utf8.end(), std::back_inserter(wide));
    } catch (const utf8::exception&) {
        return {}; // invalid UTF-8 input; the caller will fail to open the file
    }
    return wide;
}

} // namespace atom

#endif // _WIN32

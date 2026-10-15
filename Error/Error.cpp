/**
 * @file           : Error.cpp
 * @brief          : Error diagnostics and platform error capture.
**/

#include "Error.hpp"

#include <format>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cerrno>
#else
#include <cerrno>
#include <cstring>
#endif

#include <SDL3/SDL_error.h>

namespace atom {

namespace {

// Appends " key=value" for every field that is present, so a fully-populated
// error reads as one line and a minimal one stays short.
auto AppendField(std::string& out, const std::string_view key, const std::string_view value) -> void {
    if (value.empty())
        return;
    if (!out.empty())
        out.push_back(' ');
    out.append(key);
    out.push_back('=');
    out.append(value);
}

} // namespace

auto Error::Message() const -> std::string {
    std::string message{};
    message.reserve(64 + operation_.size() + resource_.size() + detail_.size());
    message.append("module=");
    message.append(ModuleName(module_));
    AppendField(message, "code", ErrorCodeName(code_));
    AppendField(message, "op", operation_);
    AppendField(message, "resource", resource_);
    AppendField(message, "detail", detail_);
    return message;
}

auto Error::MatchesModule() const noexcept -> bool {
    return ModuleOf(code_) == module_;
}

auto CaptureSdlError() -> std::string {
    const std::string message{SDL_GetError()};
    // SDL keeps the error until something clears it. Clearing here means the
    // next reader sees only its own failure instead of this one again.
    SDL_ClearError();
    return message;
}

auto CaptureErrnoMessage() -> std::string {
#if defined(_WIN32)
    return std::format("errno {}", errno);
#else
    return std::string{std::strerror(errno)};
#endif
}

auto CaptureLastSystemError() -> std::string {
#if defined(_WIN32)
    const DWORD code = GetLastError();
    if (code == 0)
        return {};

    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                            FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) {
        // No system text for this code; keep the number so it is still
        // searchable instead of silently returning nothing.
        return std::format("Win32 error {}", code);
    }

    // FormatMessageW writes CR/LF pairs and a trailing period. The message is
    // an ASCII-ish single line, so narrowing directly is safe and keeps this
    // helper free of the Utf8 module.
    std::string message{};
    message.reserve(length);
    for (DWORD index = 0; index < length; ++index) {
        const wchar_t character = buffer[index];
        if (character == L'\r' || character == L'\n')
            continue;
        message.push_back(character <= 0x7F ? static_cast<char>(character) : '?');
    }
    LocalFree(buffer);

    while (!message.empty() && (message.back() == ' ' || message.back() == '.'))
        message.pop_back();

    return std::format("Win32 error {}: {}", code, message);
#else
    return {};
#endif
}

} // namespace atom

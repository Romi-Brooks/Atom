// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Backend/SDL3/Core/SdlError.hpp>

#include <SDL3/SDL_error.h>

namespace atom::backend::sdl3 {

auto CaptureError() -> std::string {
    std::string message{SDL_GetError()};
    SDL_ClearError();
    return message;
}

auto PeekError() -> std::string {
    return std::string{SDL_GetError()};
}

auto FormatErrorSuffix() -> std::string {
    const std::string message{SDL_GetError()};
    if (message.empty())
        return {};
    std::string suffix{"; sdl="};
    suffix.append(message);
    return suffix;
}

} // namespace atom::backend::sdl3

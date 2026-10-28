// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file SdlError.hpp
 * @brief SDL3 error-slot capture, owned by the SDL3 backend instead of Atom_Error.
 *
 * SDL keeps one process-wide error string that the next SDL call overwrites, so
 * it must be copied at the failure site. That makes it a *backend* concern: the
 * engine's Error value (Atom_Error) is deliberately platform-free so Filesystem,
 * Asset and ECS can be linked and tested without any SDL backend compiled in
 * (A0.3). Backends that call SDL include this header and hand the captured text
 * to `Error::WithDetail`.
 */

#ifndef ATOM_BACKEND_SDL3_SDLERROR_HPP
#define ATOM_BACKEND_SDL3_SDLERROR_HPP

#include <string>

namespace atom::backend::sdl3 {

// Copies SDL_GetError() and clears the slot. Returns an empty string when SDL
// has no error recorded.
//
// Clearing is part of the contract: the next reader must see only its own
// failure instead of re-reading this one.
[[nodiscard]] auto CaptureError() -> std::string;

// Same capture, but keeps the slot intact. Use when the caller cannot know
// whether another SDL call will be made first and the message must survive.
[[nodiscard]] auto PeekError() -> std::string;

// Formats a captured message as "; sdl=<message>" for use as an Error detail
// suffix. Returns an empty string when there is nothing recorded, so callers can
// append unconditionally without producing trailing noise.
[[nodiscard]] auto FormatErrorSuffix() -> std::string;

} // namespace atom::backend::sdl3

#endif // ATOM_BACKEND_SDL3_SDLERROR_HPP

/**
 * @file           : Sdl3ErrorTest.cpp
 * @brief          : A0.3 — SDL error capture lives in the SDL3 backend, not in Atom_Error.
 *
 * Covers the two properties callers rely on:
 *   - CaptureError() reads the slot and clears it, so the next reader sees its
 *     own failure instead of a stale one;
 *   - PeekError()/FormatErrorSuffix() never clear, and the suffix is empty when
 *     nothing was recorded (so it can be appended unconditionally).
 */

#include <Backend/SDL3/Core/SdlError.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <string>

#include <SDL3/SDL_error.h>

auto main() -> int {
    using atom::backend::sdl3::CaptureError;
    using atom::backend::sdl3::FormatErrorSuffix;
    using atom::backend::sdl3::PeekError;

    // --- 1. Empty slot yields an empty capture, not a stale message -------
    SDL_ClearError();
    ATOM_CHECK(CaptureError().empty());
    ATOM_CHECK(PeekError().empty());
    ATOM_CHECK(FormatErrorSuffix().empty());

    // --- 2. Capture reads the message and clears the slot -----------------
    SDL_SetError("synthetic failure %d", 42);
    const std::string captured = CaptureError();
    ATOM_CHECK(captured == "synthetic failure 42");
    // Clearing is what keeps the next reader from re-reporting this failure.
    ATOM_CHECK(CaptureError().empty());

    // --- 3. Peek keeps the slot intact ------------------------------------
    SDL_SetError("peeked %s", "failure");
    ATOM_CHECK(PeekError() == "peeked failure");
    ATOM_CHECK(PeekError() == "peeked failure");

    // --- 4. The formatted suffix carries the message and leaves it intact --
    const std::string suffix = FormatErrorSuffix();
    ATOM_CHECK(suffix == "; sdl=peeked failure");
    ATOM_CHECK(SDL_GetError() != nullptr);

    SDL_ClearError();
    return 0;
}

// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "ApplicationPaths.hpp"

#include <SDL3/SDL_filesystem.h>

#include <Backend/SDL3/Core/SdlError.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom::platform {

auto ApplicationBasePath() -> Result<std::filesystem::path> {
    if (const auto* path = SDL_GetBasePath(); path && *path)
        return PathFromUtf8(path);
    return std::unexpected{
        MakeError(ErrorCode::FsIoError, Module::Fs, "ApplicationBasePath")
            .WithDetail(backend::sdl3::CaptureError())
    };
}

} // namespace atom::platform

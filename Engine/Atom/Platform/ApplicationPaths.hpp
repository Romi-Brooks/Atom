// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#ifndef ATOM_PLATFORM_APPLICATIONPATHS_HPP
#define ATOM_PLATFORM_APPLICATIONPATHS_HPP

#include <filesystem>

#include <Atom/Core/Error/Error.hpp>

#include <Atom/Build/PlatformExport.hpp>

namespace atom::platform {

// Base directory for read-only application resources. This is normally the
// executable directory on desktop and may be a resource directory in an app bundle.
// Available before window or audio initialization.
[[nodiscard]] ATOM_PLATFORM_API auto ApplicationBasePath() -> Result<std::filesystem::path>;

} // namespace atom::platform

#endif

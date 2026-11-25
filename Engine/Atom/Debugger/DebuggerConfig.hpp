// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file DebuggerConfig.hpp
 * @brief Compile-time switch for the ImGui debug overlay.
 *
 * Default: enabled in Debug, stripped in Release (NDEBUG).
 * Force either way by defining ATOM_ENABLE_DEBUGGER=0 or 1 before this
 * header (or via the compiler command line / CMake).
 *
 * When 0, Attach/Get/Overlay registration are no-ops — no ImGui frame is
 * created and no panel is drawn at runtime.
 */

#ifndef ATOM_DEBUGGER_DEBUGGER_CONFIG_HPP
#define ATOM_DEBUGGER_DEBUGGER_CONFIG_HPP

#ifndef ATOM_ENABLE_DEBUGGER
    #ifdef NDEBUG
        #define ATOM_ENABLE_DEBUGGER 0
    #else
        #define ATOM_ENABLE_DEBUGGER 1
    #endif
#endif

#endif // ATOM_DEBUGGER_DEBUGGER_CONFIG_HPP

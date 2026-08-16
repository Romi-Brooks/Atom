/**
* @file           : AtomLogChannels.hpp
  * @author         : Romi Brooks
  * @brief          : Engine log channel form
  * @attention      : Normally included via LogSystem.hpp
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ATOMLOGCHANNELS_HPP
#define ATOM_ATOMLOGCHANNELS_HPP

// Self Dependency
#include "LogChannelMacros.hpp"

// Engine log channel form. Design and usage: see Log/Doc/LogSystem.md.
// One ATOM_DEFINE_CHANNELS block per domain: (CppName, "DisplayName"),
// output = domain prefix + DisplayName + " -> ". Each domain holds up to 64 channels.

//Level-1
ATOM_DEFINE_CHANNELS(atom::core, LogChannel, "Atom.",
    (MAIN, "Main"),
    (LOGGER, "Logger"),
    (FILESYSTEM, "Filesystem"),
    (LUA, "Lua"),
    (VIDEO, "Video"),
    (WINDOW, "Window"),
    (SCREEN, "Screen"),
    (SCREEN_MANAGER, "Screen.Manager"),
    (MOVEMENT, "Movement"),
    (ENTITY, "Entity")
)

// Level-2
ATOM_DEFINE_CHANNELS(atom::entity, LogChannel, "Atom.Entity.",
    (NPC, "NPC"),
    (PLAYER, "Player")
)

// Level-2
ATOM_DEFINE_CHANNELS(atom::audio, LogChannel, "Atom.Audio.",
    (MUSIC, "Music"),
    (SFX, "SFX"),
    (PLUG_MUSICFADE, "Plug.MusicFade"),
    (MINIMP3, "Minimp3"),
    (WAVPROF, "WavProf"),
    (METADATA, "Metadata")
)

// Level-2
ATOM_DEFINE_CHANNELS(atom::backend, LogChannel, "Atom.Backend.",
    (RUNTIME, "Runtime")
)

// Level-3
ATOM_DEFINE_CHANNELS(atom::backend::sdl, LogChannel, "Atom.SDL.Backend.",
    (AUDIO, "Audio"),
    (VIDEO, "Video"),
    (RENDER, "Render"),
    (WINDOW, "Window")
)

// Level-2
ATOM_DEFINE_CHANNELS(atom::utilities, LogChannel, "Atom.Utilities.",
    (PACKAGER, "Packager")
)

#endif // ATOM_ATOMLOGCHANNELS_HPP

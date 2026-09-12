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

// Level-1
ATOM_DEFINE_CHANNELS(atom::log::core, Channel, "Atom.", (Main, "Main"), (Logger, "Logger"),
                     (Filesystem, "Filesystem"), (Lua, "Lua"), (Video, "Video"), (Window, "Window"),
                     (Screen, "Screen"), (ScreenManager, "Screen.Manager"), (Movement, "Movement"),
                     (Entity, "Entity"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::entity, Channel, "Atom.Entity.", (Npc, "NPC"), (Player, "Player"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::audio, Channel, "Atom.Audio.", (Music, "Music"), (Sfx, "SFX"),
                     (PlugMusicFade, "Plug.MusicFade"), (Minimp3, "Minimp3"), (WavProf, "WavProf"),
                     (SDL3Wav, "SDL3Wav"), (Metadata, "Metadata"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::backend, Channel, "Atom.Backend.", (Runtime, "Runtime"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::layout, Channel, "Atom.Layout.", (Core, "Core"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::debugger, Channel, "Atom.Debugger.", (ImGui, "ImGui"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::render, Channel, "Atom.Render.", (Renderer2D, "Renderer2D"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::image, Channel, "Atom.Image.", (Decoder, "Decoder"))

// Level-3
// Backend channels are grouped by engine capability first, then by concrete
// backend identifier. This lets an audio-only log filter include every audio
// backend while keeping SDL3 and SDL3_mixer distinguishable.
ATOM_DEFINE_CHANNELS(atom::log::backend::Audio, Channel, "Atom.Backend.Audio.", (sdl3, "SDL3"),
                     (sdl3_mixer, "SDL3_mixer"))

// Legacy SDL3 backend categories that have not yet been moved to the
// capability-first hierarchy.
ATOM_DEFINE_CHANNELS(atom::log::backend::sdl3, Channel, "Atom.SDL3.Backend.", (Video, "Video"), (Render, "Render"),
                     (Window, "Window"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::utilities, Channel, "Atom.Utilities.", (Packager, "Packager"))

#endif // ATOM_ATOMLOGCHANNELS_HPP

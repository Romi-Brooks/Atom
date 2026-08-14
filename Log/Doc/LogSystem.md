# LogSystem - Logging System Usage Guide

[English](LogSystem.md) | [中文](LogSystem-CN.md)

***

## Overview

`LogSystem` is the logging system for the Atom engine. It supports level-based log output and uses `LogChannel` to differentiate log sources across modules.

---

## Using LogChannel

With `LogChannel`, you can:

- Use **built-in engine channels** provided as static constants (e.g., `LogChannel::ATOM_ENTITY`)
- Create **custom game channels** by constructing `LogChannel` instances directly — no engine source modification required

### Built-in Engine Channels

```cpp
// Use directly, no extra setup needed
LOG_INFO(atom::LogChannel::ATOM_ENTITY, "Entity created");
LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX not found");
LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Pack failed");
```

Complete list of built-in channels:

| Channel Constant | Display Name |
|---|---|
| `ATOM_ENTITY` | Atom.Entity |
| `ATOM_ENTITY_NPC` | Atom.Entity.NPC |
| `ATOM_ENTITY_PLAYER` | Atom.Entity.Player |
| `ATOM_CONFIG_MOVEMENT` | Atom.Movement |
| `ATOM_FILESYSTEM` | Atom.Filesystem |
| `ATOM_MAIN` | Atom.Main |
| `ATOM_LUA` | Atom.Lua |
| `ATOM_AUDIO_MUSIC` | Atom.Audio.Music |
| `ATOM_AUDIO_SFX` | Atom.Audio.SFX |
| `ATOM_AUDIO_PLUG_MUSICFADE` | Atom.Audio.Plug.MusicFade |
| `ATOM_BACKEND_RUNTIME` | Atom.Backend.Runtime |
| `ATOM_VIDEO` | Atom.Video |
| `SDL_BACKEND_AUDIO` | SDL.Backend.Audio |
| `SDL_BACKEND_VIDEO` | SDL.Backend.Video |
| `SDL_BACKEND_RENDER` | SDL.Backend.Render |
| `SDL_BACKEND_WINDOW` | SDL.Backend.Window |
| `ATOM_WINDOW` | Atom.Window |
| `ATOM_SCREEN` | Atom.Screen |
| `ATOM_SCREEN_MANAGER` | Atom.Screen.Manager |
| `ATOM_UTILITIES_PACKAGER` | Atom.Utilities.Packager |

The channel list will be updated as the engine evolves.

### Custom Game Channels

No need to modify engine code — just construct a channel directly:

#### Define as Constants (Recommended)

Create your own header file in your game project:

```cpp
// This file belongs to your game project
#pragma once
#include "Log/LogSystem.hpp"

namespace game {
    const atom::LogChannel GAME_NPC("Game.NPC");
    const atom::LogChannel GAME_PLAYER("Game.Player");
    const atom::LogChannel GAME_SCREEN("Game.Screen");
    const atom::LogChannel GAME_MAIN("Game.Main");
}
```

Usage:

```cpp
#include "GameLogChannels.hpp"

LOG_INFO(game::GAME_NPC, "NPC spawned");
LOG_ERROR(game::GAME_PLAYER, "Failed to save");
```

#### Ad-hoc Usage (Not Recommended)

```cpp
LOG_INFO(atom::LogChannel("Game.NPC"), "NPC dialog started");
LOG_INFO(atom::LogChannel("Game.Player"), "Player save game");
```

Output:

```
[2026-01-01 12:00:00] [INFO] Game.NPC -> NPC dialog started
[2026-01-01 12:00:01] [INFO] Game.Player -> Player save game
```

The channel name automatically appends ` -> ` as a display suffix.

---

## Log Levels

| Level | Macro | Description |
|---|---|---|
| INFO | `LOG_INFO(channel, msg)` | General information |
| WARNING | `LOG_WARNING(channel, msg)` | Warning |
| ERROR | `LOG_ERROR(channel, msg)` | Error |
| DEBUG | `LOG_DEBUG(channel, msg)` | Debug information |

Examples:

```cpp
LOG_INFO(atom::LogChannel::ATOM_MAIN, "Engine started");
LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "File not found: " + filename);
LOG_ERROR(atom::LogChannel::ATOM_LUA, "Script error: " + errorMsg);
LOG_DEBUG(atom::LogChannel::ATOM_ENTITY, "Entity id: " + std::to_string(id));
```

---

## Setting the Log Display Level

Use `SetViewLogLevel` to filter logs by severity:

```cpp
// Only show WARNING and above
atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_WARNING);
```

Priority order: `DEBUG < INFO < WARNING < ERROR`

---

## Notes

1. `LogChannel` constructor takes a `std::string`. Use short, meaningful names.
2. Channel names are display strings. Atom does not normalize their case, so use a consistent style.
3. Custom channels do not require registration or prior declaration — create and use them on the fly.
4. Log output is serialized by an internal mutex. Changing the view level concurrently with logging is not yet guaranteed to be thread-safe.

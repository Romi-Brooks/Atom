# LogSystem - Logging System Usage Guide

[English](LogSystem.md) | [中文](LogSystem-CN.md)

***

## Overview

`LogSystem` is the logging system for the Atom engine. It supports level-based log output and uses **hierarchical channel domains** to differentiate log sources across modules.

Each domain is declared with a single macro (`ATOM_DEFINE_CHANNELS`) that generates, at compile time, an enum + name mapping + display prefix:

- **Engine domains**: `atom::core::LogChannel`, `atom::audio::LogChannel`, `atom::entity::LogChannel`, `atom::backend::LogChannel`, `atom::backend::sdl3::LogChannel`, `atom::utilities::LogChannel` — output prefixed with `Atom.` / `Atom.Audio.` / `Atom.Entity.` / ...
- **Game domains**: e.g. `game::GameLogChannel` — output prefixed with `Game.` (the game creates its own)

No runtime registration is needed — the `LOG_*` macros resolve any domain's enum automatically (ADL). Display prefixes make hierarchical filtering trivial.

---

## Using Channels

### Engine Channels (Built-in)

Defined in `Log/AtomLogChannels.hpp`, organized as hierarchical domains:

```cpp
// Level-1 domain: atom::core, prefix "Atom."
ATOM_DEFINE_CHANNELS(atom::core, LogChannel, "Atom.",
    (MAIN, "Main"),
    (LOGGER, "Logger"),
    // ...
)

// Level-2 domain: atom::audio, prefix "Atom.Audio."
ATOM_DEFINE_CHANNELS(atom::audio, LogChannel, "Atom.Audio.",
    (MUSIC, "Music"),
    (SFX, "SFX"),
    // ...
)

// Level-3 domain: atom::backend::sdl3, prefix "Atom.SDL3.Backend."
ATOM_DEFINE_CHANNELS(atom::backend::sdl3, LogChannel, "Atom.SDL3.Backend.",
    (AUDIO, "Audio"),
    // ...
)
```

Use directly — IDE autocomplete and compile-time checks included:

```cpp
LOG_INFO(atom::core::LogChannel::MAIN, "Engine started");
LOG_WARNING(atom::audio::LogChannel::SFX, "SFX not found");
LOG_ERROR(atom::utilities::LogChannel::PACKAGER, "Pack failed");
```

Complete list of engine channels (grouped by domain):

| Channel | Display Name |
|---|---|
| `atom::core::LogChannel::MAIN` | Atom.Main |
| `atom::core::LogChannel::LOGGER` | Atom.Logger |
| `atom::core::LogChannel::FILESYSTEM` | Atom.Filesystem |
| `atom::core::LogChannel::LUA` | Atom.Lua |
| `atom::core::LogChannel::VIDEO` | Atom.Video |
| `atom::core::LogChannel::WINDOW` | Atom.Window |
| `atom::core::LogChannel::SCREEN` | Atom.Screen |
| `atom::core::LogChannel::SCREEN_MANAGER` | Atom.Screen.Manager |
| `atom::core::LogChannel::MOVEMENT` | Atom.Movement |
| `atom::core::LogChannel::ENTITY` | Atom.Entity |
| `atom::entity::LogChannel::NPC` | Atom.Entity.NPC |
| `atom::entity::LogChannel::PLAYER` | Atom.Entity.Player |
| `atom::audio::LogChannel::MUSIC` | Atom.Audio.Music |
| `atom::audio::LogChannel::SFX` | Atom.Audio.SFX |
| `atom::audio::LogChannel::PLUG_MUSICFADE` | Atom.Audio.Plug.MusicFade |
| `atom::audio::LogChannel::MINIMP3` | Atom.Audio.Minimp3 |
| `atom::audio::LogChannel::WAVPROF` | Atom.Audio.WavProf |
| `atom::audio::LogChannel::METADATA` | Atom.Audio.Metadata |
| `atom::backend::LogChannel::RUNTIME` | Atom.Backend.Runtime |
| `atom::backend::sdl3::LogChannel::AUDIO` | Atom.SDL3.Backend.Audio |
| `atom::backend::sdl3::LogChannel::VIDEO` | Atom.SDL3.Backend.Video |
| `atom::backend::sdl3::LogChannel::RENDER` | Atom.SDL3.Backend.Render |
| `atom::backend::sdl3::LogChannel::WINDOW` | Atom.SDL3.Backend.Window |
| `atom::utilities::LogChannel::PACKAGER` | Atom.Utilities.Packager |

To add a channel, just add one line to the form of its domain in `Log/AtomLogChannels.hpp` — no other file changes. Each domain supports up to 64 channels; you can nest domains arbitrarily deep (e.g. `atom::entity::npc`).

### Game Channels (Custom Domain)

Games never touch the engine library. Create a single header in your game project — **write the channels, one line injects everything**:

```cpp
// Game/GameChannels.hpp — everything you need on the game side
#pragma once
#include <Log/LogSystem.hpp>

// ============ Write channels + one-line injection ============
ATOM_DEFINE_CHANNELS(game, GameLogChannel, "Game.",
    (GAME_NPC, "NPC"),
    (GAME_PLAYER, "Player"),
    (GAME_MAIN, "Main")
)
```

This generates the enum, the name mapping and the `Game.` prefix — all at compile time:

```cpp
#include "Game/GameChannels.hpp"

LOG_INFO(game::GameLogChannel::GAME_NPC, "NPC spawned");     // Game.NPC -> NPC spawned
LOG_ERROR(game::GameLogChannel::GAME_PLAYER, "Save failed"); // Game.Player -> Save failed
```

You can also define multiple domains (e.g. one per game module, or nested like `game::npc::LogChannel`) — each just needs its own namespace, enum name and prefix.

### Ad-hoc Usage (Temporary Channels)

For quick experiments, pass a plain string — no declaration needed:

```cpp
LOG_INFO("Game.NPC", "NPC dialog started");
LOG_INFO("Debug.temp", "Just trying something");
```

---

## Output & Hierarchical Filtering

Every log line starts with the domain prefix, so filtering works at any level:

```
[2026-01-01 12:00:00] [INFO] Atom.Audio.Music -> Playback started
[2026-01-01 12:00:01] [INFO] Atom.Entity.NPC -> NPC spawned
[2026-01-01 12:00:02] [INFO] Game.NPC -> NPC spawned
[2026-01-01 12:00:03] [WARNING] Game.Player -> Save failed
```

- Grep `"Game."` → only your game's logs
- Grep `"Atom.Audio."` → only audio logs; `"Atom.SDL."` → only SDL backend logs
- Grep `"Atom."` → all engine logs

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
LOG_INFO(atom::core::LogChannel::MAIN, "Engine started");
LOG_WARNING(atom::audio::LogChannel::SFX, "File not found: " + filename);
LOG_ERROR(atom::core::LogChannel::LUA, "Script error: " + errorMsg);
LOG_DEBUG(atom::core::LogChannel::ENTITY, "Entity id: " + std::to_string(id));
```

---

## Setting the Log Display Level

Use `SetViewLogLevel` to filter logs by severity:

```cpp
// Only show WARNING and above
atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_WARNING);
```

Each call prints a confirmation through the `ATOM_LOGGER` channel (`atom::core::LogChannel::LOGGER`), e.g.
`Set log level to WARNING`, so the currently active level is always visible
in the console.

Priority order: `DEBUG < INFO < WARNING < ERROR`

---

## TODO / Roadmap

Domain-based filtering is the next planned feature set

- [ ] **Runtime domain filter** — e.g. `atom::Log::SetChannelFilter("Atom.Audio.", false)` to mute/keep whole domains (or per-channel), separating `Game.` logs from engine noise without grepping.
- [ ] **Per-domain view level** — e.g. keep `Atom.Audio.` at DEBUG while the rest stays at WARNING.
- [ ] **Runtime channel config file** (optional) — load display names / levels from a file so games can tune logging without recompiling.

---

## Notes

1. Channels are **enums** — typos fail at compile time, and the IDE autocompletes them.
2. Display format is `prefix + shortName + " -> "`. Engine domains use `Atom.*` prefixes; each game domain picks its own.
3. To add a channel, add one line to the form of its domain (`(CPP_NAME, "Short.Name")`). A single domain supports up to 64 channels; nesting domains is unlimited.
4. Ad-hoc channels are plain strings — no declaration or registration required.
5. Log output is serialized by an internal mutex. Changing the view level concurrently with logging is not yet guaranteed to be thread-safe.

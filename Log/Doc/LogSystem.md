# LogSystem - Logging System Usage Guide

[English](LogSystem.md) | [中文](LogSystem-CN.md)

***

## Overview

`LogSystem` is the logging system for the Atom engine. It supports level-based log output and uses **hierarchical channel domains** to differentiate log sources across modules.

Each domain is declared with a single macro (`ATOM_DEFINE_CHANNELS`) that generates, at compile time, an enum-backed value namespace, name mapping and display prefix:

- **Engine domains**: `atom::log::core`, `atom::log::audio`, `atom::log::entity`, `atom::log::render`, `atom::log::image`, `atom::log::layout`, `atom::log::debugger`, `atom::log::backend`, `atom::log::backend::Audio`, `atom::log::utilities` — output prefixed with `Atom.` / `Atom.Audio.` / `Atom.Render.` / ...
- **Game domains**: e.g. `game::log` — output prefixed with `Game.` (the game creates its own)

No runtime registration is needed — the `LOG_*` macros resolve any domain's enum automatically (ADL). Display prefixes make hierarchical filtering trivial.

---

## Using Channels

### Engine Channels (Built-in)

Defined in `Log/AtomLogChannels.hpp`, organized as hierarchical domains:

```cpp
// Level-1 domain: atom::log::core, prefix "Atom."
ATOM_DEFINE_CHANNELS(atom::log::core, Channel, "Atom.",
    (Main, "Main"),
    (Logger, "Logger"),
    // ...
)

// Level-2 domain: atom::log::audio, prefix "Atom.Audio."
ATOM_DEFINE_CHANNELS(atom::log::audio, Channel, "Atom.Audio.",
    (Music, "Music"),
    (Sfx, "SFX"),
    // ...
)

// Level-3 domain: atom::log::backend::Audio, prefix "Atom.Backend.Audio."
// The channel value is the concrete backend ID.
ATOM_DEFINE_CHANNELS(atom::log::backend::Audio, Channel, "Atom.Backend.Audio.",
    (sdl3, "SDL3"),
    (sdl3_mixer, "SDL3_mixer"),
    // ...
)
```

Use directly — IDE autocomplete and compile-time checks included:

```cpp
LOG_INFO(atom::log::core::Main, "Engine started");
LOG_WARNING(atom::log::audio::Sfx, "SFX not found");
LOG_ERROR(atom::log::utilities::Packager, "Pack failed");
```

Complete list of engine channels (grouped by domain):

| Channel | Display Name |
|---|---|
| `atom::log::core::Main` | Atom.Main |
| `atom::log::core::Logger` | Atom.Logger |
| `atom::log::core::Filesystem` | Atom.Filesystem |
| `atom::log::core::Lua` | Atom.Lua |
| `atom::log::core::Video` | Atom.Video |
| `atom::log::core::Window` | Atom.Window |
| `atom::log::core::Screen` | Atom.Screen |
| `atom::log::core::ScreenManager` | Atom.Screen.Manager |
| `atom::log::core::Movement` | Atom.Movement |
| `atom::log::core::Entity` | Atom.Entity |
| `atom::log::entity::Npc` | Atom.Entity.NPC |
| `atom::log::entity::Player` | Atom.Entity.Player |
| `atom::log::audio::Music` | Atom.Audio.Music |
| `atom::log::audio::Sfx` | Atom.Audio.SFX |
| `atom::log::audio::PlugMusicFade` | Atom.Audio.Plug.MusicFade |
| `atom::log::audio::Minimp3` | Atom.Audio.Minimp3 |
| `atom::log::audio::WavProf` | Atom.Audio.WavProf |
| `atom::log::audio::SDL3Wav` | Atom.Audio.SDL3Wav |
| `atom::log::audio::Metadata` | Atom.Audio.Metadata |
| `atom::log::render::Renderer2D` | Atom.Render.Renderer2D |
| `atom::log::image::Decoder` | Atom.Image.Decoder |
| `atom::log::layout::Core` | Atom.Layout.Core |
| `atom::log::debugger::ImGui` | Atom.Debugger.ImGui |
| `atom::log::backend::Runtime` | Atom.Backend.Runtime |
| `atom::log::backend::Audio::sdl3` | Atom.Backend.Audio.SDL3 |
| `atom::log::backend::Audio::sdl3_mixer` | Atom.Backend.Audio.SDL3_mixer |
| `atom::log::backend::sdl3::Video` | Atom.SDL3.Backend.Video |
| `atom::log::backend::sdl3::Render` | Atom.SDL3.Backend.Render |
| `atom::log::backend::sdl3::Window` | Atom.SDL3.Backend.Window |
| `atom::log::utilities::Packager` | Atom.Utilities.Packager |

To add a channel, add one `(PascalCaseName, "Display.Name")` entry to the appropriate domain in `Log/AtomLogChannels.hpp`. Each domain supports up to 64 channels; you can nest domains arbitrarily deep (e.g. `atom::log::entity::npc`). Backend implementation channels are the exception: use their canonical runtime IDs, such as `sdl3` and `sdl3_mixer`.

### Game Channels (Custom Domain)

Games never touch the engine library. Create a single header in your game project — **write the channels, one line injects everything**:

```cpp
// Game/GameChannels.hpp — everything you need on the game side
#pragma once
#include <Log/LogSystem.hpp>

// ============ Write channels + one-line injection ============
ATOM_DEFINE_CHANNELS(game::log, Channel, "Game.",
    (Npc, "NPC"),
    (Player, "Player"),
    (Main, "Main")
)
```

This generates direct values such as `game::log::Npc`, the name mapping and the `Game.` prefix — all at compile time:

```cpp
#include "Game/GameChannels.hpp"

LOG_INFO(game::log::Npc, "NPC spawned");     // Game.NPC -> NPC spawned
LOG_ERROR(game::log::Player, "Save failed"); // Game.Player -> Save failed
```

You can also define multiple domains (e.g. one per game module, or nested like `game::log::npc`) — each just needs its own namespace, `Channel` type and prefix.

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
- Grep `"Atom.Audio."` → audio-domain logs; `"Atom.Backend.Audio."` → audio backend logs
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
LOG_INFO(atom::log::core::Main, "Engine started");
LOG_WARNING(atom::log::audio::Sfx, "File not found: " + filename);
LOG_ERROR(atom::log::core::Lua, "Script error: " + errorMsg);
LOG_DEBUG(atom::log::core::Entity, "Entity id: " + std::to_string(id));
```

---

## Setting the Log Display Level

Use `SetViewLogLevel` to filter logs by severity:

```cpp
// Only show WARNING and above
atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_WARNING);
```

Each call prints a confirmation through the `ATOM_LOGGER` channel (`atom::log::core::Logger`), e.g.
`Set log level to WARNING`, so the currently active level is always visible
in the console.

Priority order: `DEBUG < INFO < WARNING < ERROR`

---

## Windows Console UTF-8

If a Windows program writes UTF-8 text to its console, call this once at the
start of `main`, before any console or log output:

```cpp
atom::Log::SetConsoleOutputUtf8();
```

On Windows this wraps `SetConsoleOutputCP(CP_UTF8)`. On other platforms it is
a no-op, so cross-platform application code needs no platform preprocessor
branches or `windows.h` include. The setting affects console output only; it
does not change console input or supply a missing font.

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

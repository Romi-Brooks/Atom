# Atom Engine
[English](README.md) | [中文](Docs/README-CN.md)

---
**Atom** is a modular **2D game engine** written in **C++23**, powered by **SDL3**.
Designed to provide a modern, clean, and lightweight development experience.

> **Stable (master)** — This branch tracks the latest beta release.  
> **Active development happens on the [`dev`](https://github.com/Romi-Brooks/Atom/tree/dev) branch.**  
> APIs and architecture are subject to change. Feedback and contributions are welcome.

---

## Quick Start

### Prerequisites

- CMake >= 3.20
- C++23 compatible compiler
- Third-party dependencies are **bundled** in `ThirdParty/`.

### Building

```bash
git clone https://github.com/Romi-Brooks/Atom.git
cd Atom
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

---

## Examples

Ready-to-run examples are located in [`Example/`](Example/).

| Example | Description |
|---------|-------------|
| `example_simple_window` | Minimal SDL3 window |
| `example_simple_window_debug` | Window with ImGui debug overlay |
| `example_audio_playback` | Music playback with fade switching |
| `example_sfx_playback` | SFX playback with voice pool (overlapping) |
| `example_media_metadata` | Read audio file metadata via TagLib |

---

### Coding Standard

See the full coding standard:
- [CODING_STANDARD](CODING_STANDARD.md)

---

## Packager Tool

Atom provides a resource packaging tool for packing/unpacking game assets into the HPKG archive format.

- [Packager Documentation](Utilities/Packager/Doc/Packager.md) — CLI usage, API reference, and examples

---

## Engine Dependencies

Atom uses **SDL3** as its multimedia abstraction layer. SDL3 is **bundled** in the repository at `ThirdParty/SDL3/`.

### Bundled Dependencies

| Library | Version | Path | Purpose |
|---------|---------|------|---------|
| [SDL3](https://github.com/libsdl-org/SDL) | 3.x | `ThirdParty/SDL3/` | Windowing, rendering, audio, input |
| [ImGui](https://github.com/ocornut/imgui) | 1.x | `ThirdParty/ImGUI/` | Debug overlay UI |
| [Lua](https://www.lua.org/) | 5.x | `ThirdParty/Lua/` | Scripting engine |
| [TagLib](https://taglib.org/) | 2.x | `ThirdParty/taglib/` | Audio metadata reading |
| [utfcpp](https://github.com/nemtrif/utfcpp) | 4.x | `ThirdParty/utfcpp/` | UTF-8 validation and conversion |

### SDL3 Deployment

SDL3 is compatible with any modern MinGW-w64 distribution (GCC 13+, UCRT).
Compilers bundled with JetBrains IDEs, WinLibs, or MSYS2 packages all work without version lock-in.

---

## Architecture

```
Atom/
├── Backend/
│   ├── Contracts/      # Render/Audio/Video backend contracts
│   ├── Registry/       # Backend-independent decoder registry
│   ├── Runtime/        # Global backend selection, defaults and hot switching
│   ├── Builtin/        # Atom-owned experimental implementations
│   └── SDL3/           # SDL3 render, window and audio implementations
├── Media/
│   ├── Audio/
│   │   ├── Mixing/     # AudioMixer and category volumes
│   │   ├── Resources/  # AudioClipLoader and AudioClipCache
│   │   ├── Playback/   # MusicPlayer, SFXPlayer and VoicePool
│   │   └── Transitions/# Frame-driven music transitions
│   └── Video/          # Video (stub)
├── Log/                # Logging system
├── Window/             # Screen system, Debugger
└── Lua/                # Lua binding
```

---

## Roadmap

- [x]  **Backend Migration** — Migrated from **SFML** to **SDL3**
- [ ] **Renderer Plugin System** — Users will be able to choose or write their own rendering backend.
- [ ] **ECS Framework** — Replace the current simple entity system with a proper Entity-Component-System architecture.
- [ ] **Tooling** — Editor, asset pipeline, and profiling tools.

---

### Acknowledgements

Atom Engine uses the following open-source libraries. We extend our sincere gratitude to their developers:

- [SDL3](https://github.com/libsdl-org/SDL) — Windowing, rendering, audio, and input
- [ImGui](https://github.com/ocornut/imgui) — Debug overlay UI
- [Lua](https://www.lua.org/) — Scripting engine
- [TagLib](https://taglib.org/) — Audio metadata reading
- [utfcpp](https://github.com/nemtrif/utfcpp) — UTF-8 validation and conversion

---

## License
This Project is licensed under the [MIT License](LICENSE)

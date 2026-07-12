# Atom Engine
[English](README.md) | [中文](Docs/README-CN.md)

---
**Atom** is a modular **2D game engine** written in **C++23**, powered by **SDL3**.
Designed to provide a modern, clean, and lightweight development experience.

> **Active Development** — Atom is still in early development. APIs and architecture are subject to change. Feedback and contributions are welcome.

---

## Quick Start

### Prerequisites

- CMake >= 3.20
- C++23 compatible compiler
- Third-party dependencies are **bundled** in `ThirdParty/Lib/`.

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
| `example_ffmepg_playback` | FFmpeg-based video playback |

---

### Coding Standard

See the full coding standard:
- [CODING_STANDARD](CODING_STANDARD.md)

---

## Packager Tool

Atom provides a resource packaging tool for packing/unpacking game assets into the HPKG archive format.

- [Packager Documentation](Utilities/Packager/Doc/README.md) — CLI usage, API reference, and examples

---

## Engine Dependencies

Atom uses **SDL3** as its multimedia abstraction layer. SDL3 is **bundled** in the repository at `ThirdParty/Lib/SDL3/`.

### Bundled Dependencies

| Library | Version | Path | Purpose |
|---------|---------|------|---------|
| [SDL3](https://github.com/libsdl-org/SDL) | 3.x | `ThirdParty/Lib/SDL3/` | Windowing, rendering, audio, input |
| [ImGui](https://github.com/ocornut/imgui) | 1.x | `ThirdParty/Lib/ImGUI/` | Debug overlay UI |
| [Lua](https://www.lua.org/) | 5.x | `ThirdParty/Lib/Lua/` | Scripting engine |
| [TagLib](https://taglib.org/) | 2.x | `ThirdParty/Lib/taglib/` | Audio metadata reading |
| [utfcpp](https://github.com/nemtrif/utfcpp) | 4.x | `ThirdParty/Lib/utfcpp/` | UTF-8 validation and conversion |
| [FFmpeg](https://ffmpeg.org/) | 7.x | `ThirdParty/Lib/FFmpeg/` | Video/audio decoding |

### SDL3 Deployment

SDL3 is pre-built and located at `ThirdParty/Lib/SDL3/x86_64-w64-mingw32/`.
The CMake configuration finds it via `find_package(SDL3 REQUIRED CONFIG)`.

SDL3 is compatible with any modern MinGW-w64 distribution (GCC 13+, UCRT).
Compilers bundled with JetBrains IDEs, WinLibs, or MSYS2 packages all work without version lock-in.

---

## Architecture

```
Atom/
├── Engine/
│   ├── Audio/          # DecoderRegistry
│   ├── Interfaces/     # Abstract interfaces (IAudioDecoder, IAudioBuffer…)
│   └── Render/         # SDL3 window/renderer wrappers
├── Media/
│   ├── Audio/
│   │   ├── Backend/    # SDL3 audio stream sources (Music, SFX, Buffer)
│   │   ├── Decoder/    # WAV decoder + AtomWavDecoderBackend
│   │   ├── Manager/    # VolumeManager, SFXManager
│   │   ├── Music/      # Music playback (domain)
│   │   ├── Plugs/      # MusicFade plugin
│   │   └── SFX/        # SFX playback with voice pool
│   └── Video/          # Video (stub)
├── Log/                # Logging system
├── Window/             # Screen system, Debugger
└── Lua/                # Lua binding
```

---

## Roadmap

- ✅ **Backend Migration** — Migrated from **SFML** to **SDL3**
- 🔄 **Renderer Plugin System** — Users will be able to choose or write their own rendering backend.
- 🔄 **ECS Framework** — Replace the current simple entity system with a proper Entity-Component-System architecture.
- 🔄 **Tooling** — Editor, asset pipeline, and profiling tools.

---

### Acknowledgements

Atom Engine uses the following open-source libraries. We extend our sincere gratitude to their developers:

- [SDL3](https://github.com/libsdl-org/SDL) — Windowing, rendering, audio, and input
- [ImGui](https://github.com/ocornut/imgui) — Debug overlay UI
- [Lua](https://www.lua.org/) — Scripting engine
- [TagLib](https://taglib.org/) — Audio metadata reading
- [utfcpp](https://github.com/nemtrif/utfcpp) — UTF-8 validation and conversion
- [FFmpeg](https://ffmpeg.org/) — Video and audio decoding

---

## License
This Project is licensed under the [MIT License](LICENSE)

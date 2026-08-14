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
- Git, for initializing the pinned source dependencies in `ThirdParty/`
- A C and C++ compiler; dependencies and Atom are built with the same toolchain

### Building

```bash
git clone --recurse-submodules https://github.com/Romi-Brooks/Atom.git
cd Atom
cmake -B build -G "MinGW Makefiles"
cmake --build build --parallel
```

The project uses standard CMake workflows on every platform. Choose a generator
and compiler that belong to the same toolchain as all dependency builds.

Windows with Ninja and MSVC (from a Visual Studio developer shell):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Windows with MinGW:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Linux with GCC and Ninja:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
```

macOS with Apple Clang and Ninja uses the same commands without the explicit
GCC compiler options. IDEs with CMake integration can configure the repository
directly; no IDE-specific project files are required.

SDL3, TagLib, Dear ImGui, Lua and utfcpp are pinned Git submodules. CMake builds
them from source with the same compiler and ABI as Atom. If the repository was
cloned without the dependencies, run `git submodule update --init` before
configuring CMake.

For dependency setup and troubleshooting, see the
[Chinese Linux build guide](Docs/Linux-Build-CN.md).

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

Atom provides a resource packaging tool for packing/unpacking game assets into the APKG archive format.

- [Packager Documentation](Utilities/Packager/Doc/Packager.md) — CLI usage, API reference, and examples

---

## Engine Dependencies

Atom uses **SDL3** as its multimedia abstraction layer. All third-party
dependencies are pinned source submodules and are built by
`ThirdParty/CMakeLists.txt` before the engine targets that use them.
The exact upstream URLs and locked commits are recorded in
[`ThirdParty/README.md`](ThirdParty/README.md).

### Source Dependencies

| Library | Version | Path | Purpose |
|---------|---------|------|---------|
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.12 | `ThirdParty/SDL3/` | Windowing, rendering, audio, input |
| [ImGui](https://github.com/ocornut/imgui) | 1.92.9 | `ThirdParty/ImGUI/` | Debug overlay UI |
| [Lua](https://www.lua.org/) | 5.4.7 | `ThirdParty/Lua/` | Scripting engine |
| [TagLib](https://taglib.org/) | 2.1.1 | `ThirdParty/taglib/` | Audio metadata reading |
| [utfcpp](https://github.com/nemtrif/utfcpp) | 4.0.8 | `ThirdParty/utfcpp/` | UTF-8 validation and conversion |

### Dependency Builds

Dependencies are built as static libraries by default. The first clean build is
therefore longer, while later builds reuse CMake and compiler outputs. No
precompiled Windows libraries are reused on Linux or with another compiler.

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
├── Lua/                # Lua binding
└── ThirdParty/         # Pinned source submodules and dependency CMake entry
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

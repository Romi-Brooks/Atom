# Atom Engine
[English](README.md) | [中文](Docs/README-CN.md)

---
**Atom** is a modular **2D/3D game-engine foundation** written in **C++23**, powered by **SDL3** and **SDL_GPU**.
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
- Vulkan shader tools providing `glslc` and `spirv-val` (via `VULKAN_SDK` or `PATH`)
- On Windows, DXC and `spirv-cross` for the default D3D12 shader output
- On macOS, `spirv-cross` for the default Metal shader output

Linux builds generate SPIR-V only and do not require DXC. Tool locations may be
provided as local CMake cache hints with `ATOM_VULKAN_SDK_ROOT` and
`ATOM_DXC_ROOT`; never commit machine-specific paths.

### Building

Clone the repository with its pinned dependencies first:

```bash
git clone --recurse-submodules https://github.com/Romi-Brooks/Atom.git
cd Atom
```

Windows (MSVC, run from a Visual Studio Developer PowerShell):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Windows (MinGW):

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Linux (GCC and Ninja):

Install the build and SDL3 development packages first (Ubuntu 22.04+):

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential git make cmake ninja-build pkg-config \
  glslc spirv-tools \
  libasound2-dev libpulse-dev libx11-dev libxext-dev \
  libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxtst-dev \
  libxss-dev libxkbcommon-dev libdrm-dev libgbm-dev \
  libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libdbus-1-dev \
  libfribidi-dev libibus-1.0-dev libthai-dev \
  libudev-dev libusb-1.0-0-dev libwayland-dev \
  wayland-protocols libdecor-0-dev
```

`glslc` compiles the GLSL sources to SPIR-V and `spirv-tools` provides
`spirv-val`. SDL3's optional X11/Wayland/audio features use the remaining
development packages; unavailable optional drivers are disabled by SDL3.

Configure and build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
```

Third-party source dependencies are built from `ThirdParty/` with the same compiler. If the
submodules were not downloaded, run `git submodule update --init` before CMake.

SDL3, TagLib, Dear ImGui, Lua, utfcpp and minimp3 are pinned Git submodules;
the two small stb headers are vendored with recorded versions and hashes.
CMake builds the source dependencies with the same compiler and ABI as Atom. If the
repository was cloned without the dependencies, run `git submodule update --init`
before configuring CMake.

Architecture and development references:

- [API guide (Chinese)](Docs/API-Guide-CN.md)
- [Unified remaining-work list (Chinese)](Docs/Remaining-Issues.md)

---

## Examples

Ready-to-run examples are located in [`Example/`](Example/).

| Example | Description |
|---------|-------------|
| `Example_Simple_Window` | Minimal SDL3 + SDL_GPU window |
| `Example_Simple_Window_Debug` | SDL_GPU-backed ImGui debugger overlay |
| `Example_SDL_GPU_Clear_Smoke` | Custom shaders, textured 2D quad, and depth-tested 3D mesh |
| `Example_Renderer2D_Smoke` | Batched primitives, atlas/source rects, decoded PNG, clipping, layers, and camera |
| `Example_MusicCard` | Yoga music card using Renderer2D, stb image/font data, and an ImGui debugger; scans `E:\Music` by default (optional directory argument) |
| `Example_Audio_Playback` | Music playback with fade switching |
| `Example_SFX_Playback` | SFX playback with voice pool (overlapping) |
| `Example_Audio_Metadata` | Read audio tags/properties via the engine's AudioMetadataReader |
| `Example_Packaged_Music` | Pack resources → load into memory → stream-play (MP3/WAV) |

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

Atom uses **SDL3** as its multimedia abstraction layer. Third-party source
dependencies are pinned as submodules or vendored files and are built by
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
| [minimp3](https://github.com/lieff/minimp3) | master `ea99364` | `ThirdParty/minimp3/` | MP3 decoding (header-only) |
| [stb](https://github.com/nothings/stb) | image 2.30 / truetype 1.26 | `ThirdParty/stb/` | Image decoding and trusted-font rasterization |

### Dependency Builds

Dependencies are built as static libraries by default. The first clean build is
therefore longer, while later builds reuse CMake and compiler outputs. No
precompiled Windows libraries are reused on Linux or with another compiler.

---

## Roadmap

- [x] **SDL_GPU foundation** — SDL chooses D3D12, Vulkan, or Metal; custom GLSL shaders and minimal 2D/3D validation are available.
- [x] **Renderer2D and debugger migration** — Batched 2D, image/font atlases, MusicCard, and ImGui now use SDL_GPU; no Atom target builds the SDL_Renderer path.
- [ ] **Native Vulkan backend** — Reuse the same RHI and SPIR-V assets after the SDL_GPU renderer stabilizes.
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
- [minimp3](https://github.com/lieff/minimp3) — MP3 decoding
- [stb](https://github.com/nothings/stb) — Image decoding and trusted-font rasterization

---

## License
This Project is licensed under the [MIT License](LICENSE)

# Atom Engine
[English](README.md) | [中文](Docs/Community/README-CN.md)

---
**Atom** is a modular **2D/3D game-engine foundation** written in **C++23**, powered by **SDL3** and **SDL_GPU**.
Designed to provide a modern, clean, and lightweight development experience.

> **Stable branch (master)** — The first beta release is in preparation.
>
> **Active development happens on the [`dev`](https://github.com/Romi-Brooks/Atom/tree/dev) branch.**
>
> APIs and architecture are subject to change. Feedback and contributions are welcome.

---

## Quick Start

### Prerequisites

- CMake >= 3.20
- Ninja (for the Ninja build commands below)
- Git, for initializing the pinned source dependencies in `ThirdParty/`
- A C compiler and a C++23-capable C++ compiler
- Vulkan shader tools providing `glslc` and `spirv-val` (via `VULKAN_SDK` or `PATH`)
- On Windows, DXC and `spirv-cross` for the default D3D12 shader output
- On macOS, `spirv-cross` for the default Metal shader output

Linux builds generate SPIR-V only and do not require DXC. Tool locations may be
set with `ATOM_VULKAN_SDK_ROOT` and `ATOM_DXC_ROOT`. To use existing shader binaries,
set `ATOM_PRECOMPILED_SHADER_DIR`.

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

Install the build and SDL3 development packages first (Ubuntu):

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
development packages. SDL3 disables optional drivers whose dependencies are unavailable.

Configure and build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
```

If the submodules were skipped during cloning, run `git submodule update --init --recursive` before configuring CMake.

### Running Tests

Atom's tests build by default. Set `ATOM_BUILD_TESTS=OFF` to disable them.

```bash
ctest --test-dir build --output-on-failure --timeout 90
```

### Layout and API

`Engine/` groups runtime, platform, rendering, world, and resource modules by domain.
`Example/` contains runnable demos, `Tools/` contains standalone tools, and `Test/` contains regression tests.
Applications link the module targets they need, such as `Atom_Runtime` and `Atom_World2D`.
Public headers keep their module paths, such as `<Runtime/Application.hpp>`.
Use `atom::platform::ApplicationBasePath()` to locate app resources, `JobSystem::ScheduleOrRun()`
to submit work without dependencies, and `atom::random::Rng` for seeded random streams.

- [API guide (Chinese)](Docs/Community/API-Guide-CN.md) — `Application` / `Screen`, input, `UiTree`, audio, and ECS
- [Beta milestones and remaining work (Chinese)](Docs/Issues/Remaining-Issues.md#9-beta-准备与里程碑)
- [Subsystem designs (Chinese)](Docs/Plans/Specs/Subsystem-Plans-CN.md)
- [Scene v2](Docs/Plans/Specs/Scene-Schema-CN.md) · [Sprite Atlas v1](Docs/Plans/Specs/Sprite-Atlas-CN.md) · [Tilemap](Docs/Plans/Specs/Tilemap-Schema-CN.md) · [ECS components](Docs/Plans/Specs/ECS-Component-Vocabulary-CN.md) (Chinese)

---

## Examples

Ready-to-run examples are located in [`Example/`](Example/).

| Example | Description |
| --- | --- |
| `Example_Simple_Window` | Minimal SDL3 + SDL_GPU window |
| `Example_Simple_Window_Debug` | SDL_GPU window with an ImGui debugger overlay |
| `Example_Input_Demo` | InputSystem, action bindings, and mouse input |
| `Example_UI_Runtime_Demo` | Retained UI with `UiTree` and screen transitions |
| `Example_Physics_Demo` | Interactive 2D physics |
| `Example_ECS_Tour` | Entity and component storage and queries |
| `Example_Job_System_Tour` | Job submission, dependencies, and waiting |
| `Example_Lua_Scripting` | Load a Lua script and control audio playback |
| `Example_MusicCard` | Yoga and Renderer2D music player. Takes a music directory argument and defaults to `E:\Music` |
| `Example_Audio_Playback` | Music playback with fades |
| `Example_SFX_Playback` | Overlapping sound effects through a voice pool |
| `Example_Audio_Metadata` | Read audio tags and properties with `AudioMetadataReader` |
| `Example_Packaged_Music` | Read MP3/WAV entries from an APKG archive into memory for playback |
| `Example_Homeless` | [Playable pixel-art demo](Example/Homeless/README-CN.md) with 2D camera, sprite animation, particles, and audio |

Set `-DATOM_ENABLE_DEBUGGER=ON` to enable the debugger overlay in Release builds.
Terminal logs are on by default in Debug, RelWithDebInfo, and builds with that option enabled.
On Windows, windowed examples launch without a console in standard Release builds.

---

### Coding Standard

See the full coding standard.

- [CODING_STANDARD](CODING_STANDARD.md)

---

## Packager Tool

Atom Packager reads and writes **APKG v2** archives with directory trees, asset types, and content verification.
`packager_tool` opens the GUI by default. Use `--tui` for the terminal UI or `--path <package.apkg>` to open an archive there.
Scripts can use `pack`, `list`, and `verify`.

Engine code can build archives with `PackageBuilder` and mount them for streaming with `Vfs::MountPackage`.

- [Packager documentation](Engine/Atom/Resources/Utilities/Packager/Doc/Packager.md) — GUI, command line, and API

---

## Engine Dependencies

Atom uses **SDL3** as its multimedia abstraction layer. Third-party source
dependencies are pinned as submodules and are built by `ThirdParty/CMakeLists.txt`
before the engine targets that use them.
The exact upstream URLs and locked commits are recorded in
[`ThirdParty/README.md`](ThirdParty/README.md).

### Source Dependencies

| Library | Version | Path | Purpose |
| --- | --- | --- | --- |
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.12 | `ThirdParty/SDL3/` | Windowing, rendering, audio, and input |
| [SDL_mixer](https://github.com/libsdl-org/SDL_mixer) | 3.2.4 | `ThirdParty/SDL_mixer/` | Mixes PCM decoded by Atom |
| [ImGui](https://github.com/ocornut/imgui) | 1.92.9 | `ThirdParty/ImGUI/` | Debug overlay UI |
| [Lua](https://www.lua.org/) | 5.4.7 | `ThirdParty/Lua/` | Scripting |
| [TagLib](https://taglib.org/) | 2.1.1 | `ThirdParty/taglib/` | Audio metadata |
| [utfcpp](https://github.com/nemtrif/utfcpp) | 4.0.8 | `ThirdParty/utfcpp/` | UTF-8 validation and conversion |
| [minimp3](https://github.com/lieff/minimp3) | `ea99364` | `ThirdParty/minimp3/` | MP3 decoding |
| [stb](https://github.com/nothings/stb) | `2c980bb` | `ThirdParty/stb/` | Image decoding |
| [Yoga](https://github.com/facebook/yoga) | 3.2.1 | `ThirdParty/Yoga/` | Flexbox layout |
| [FreeType](https://github.com/freetype/freetype) | 2.13.3 | `ThirdParty/FreeType/` | Font loading and glyph rasterization |
| [HarfBuzz](https://github.com/harfbuzz/harfbuzz) | 10.4.0 | `ThirdParty/HarfBuzz/` | OpenType text shaping |
| [EnTT](https://github.com/skypjack/entt) | 4.0.0 | `ThirdParty/EnTT/` | Entity and component storage |

### Dependency Builds

Dependencies are built as static libraries by default. The first clean build is
therefore longer, while later builds reuse CMake and compiler outputs.

---

## Roadmap

- [x] **SDL_GPU foundation** — Shader outputs for D3D12, Vulkan, and Metal, with basic 2D and 3D drawing.
- [x] **Renderer2D and debugger** — Batched sprites, font atlases, post-processing, and ImGui debug panels.
- [x] **ECS and 2D world** — EnTT storage, scene loading, tilemaps, 2D physics, cameras, frame animation, and particles.
- [x] **Resource tools** — APKG v2 packaging, inspection, and VFS mounting, with GUI and terminal modes.
- [ ] **First beta SDK** — Shared library installation and CMake exports, runtime asset deployment, and cross-platform external-project validation.
- [ ] **Native Vulkan backend and broader tooling** — See the [remaining work](Docs/Issues/Remaining-Issues.md).

---

### Acknowledgements

Thanks to the following open-source projects and their contributors.

- [SDL3](https://github.com/libsdl-org/SDL) and [SDL_mixer](https://github.com/libsdl-org/SDL_mixer)
- [ImGui](https://github.com/ocornut/imgui) and [Yoga](https://github.com/facebook/yoga)
- [FreeType](https://github.com/freetype/freetype) and [HarfBuzz](https://github.com/harfbuzz/harfbuzz)
- [Lua](https://www.lua.org/) and [EnTT](https://github.com/skypjack/entt)
- [TagLib](https://taglib.org/), [utfcpp](https://github.com/nemtrif/utfcpp), [minimp3](https://github.com/lieff/minimp3), and [stb](https://github.com/nothings/stb)

---

## License
This Project is licensed under the [MIT License](LICENSE)

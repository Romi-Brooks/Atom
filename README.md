> [!WARNING]
> This branch was forked from the original Atom Engine. Its window renderer uses SFML3
> The new version has been migrated to SDL3 and resides on the master branch.  
> Note that this branch may be deprecated in subsequent releases!

# Atom Engine
[English](README.md) | [中文](Docs/README-CN.md)

---
**Atom** is a modular **2D game engine** written in **C++23**, designed to provide a modern, clean, and lightweight development experience.

> **Active Development** — Atom is still in early development. APIs and architecture are subject to change. Feedback and contributions are welcome.

---

## Quick Start

### Prerequisites

- CMake >= 3.20
- C++23 compatible compiler
- Third-party dependencies (SFML, etc.) are **not** bundled — download them from official sources (see [Engine Dependencies](#engine-dependencies)).

---

## Examples

Ready-to-run examples are located in [`Example/`](Example/).

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

The Atom engine currently uses **SFML 3.0.0** as its multimedia library.

### SFML Deployment

This project follows the official SFML download guide: [SFML 3.0.0 Download](https://www.sfml-dev.org/download/sfml/3.0.0/)

Please refer to the official guide above for toolchain download instructions for each platform.

#### Windows Build Support

**Supported Compilers:**

| Platform | Compiler |
|---|---|
| 32-bit | GCC 14.2.0 MinGW (DW2) (UCRT) |
| 64-bit | GCC 14.2.0 MinGW (SEH) (UCRT) |

**Important:** Compiler versions must match 100%! When using MinGW packages, GCC versions that seem to match are not sufficient. You must use one of the following matching compilers:

- WinLibs UCRT 14.2.0 (32-bit)
- WinLibs UCRT 14.2.0 (64-bit)

#### Linux Build Support

On Linux, if you have a 64-bit OS, the 64-bit toolchain is installed by default.

**Supported Compilers:**

| Platform | Compiler |
|---|---|
| 64-bit | GCC - 64-bit |

**Notes:**

- Compiling for 32-bit requires installing specific packages and/or using specific compiler options
- If you require a 32-bit build of SFML, you will need to build it yourself
- **Recommended:** Use the SFML version from your package manager (if recent enough) or build from source to prevent incompatibilities

#### macOS Build Support

**Install via Homebrew:**

```bash
brew install sfml@3.0.0
```

**Supported Compilers:**

| Platform | Compiler |
|---|---|
| 64-bit | Clang - 64-bit |
| ARM64 | Clang - ARM64 |

---

## Future Roadmap

Atom is planned to evolve significantly:

- **Backend Migration**: Decouple from **SFML** and migrate to **SDL3** to deliver improved underlying performance, alongside native support for multi-backend renderers including **Vulkan**, **OpenGL**, and **DirectX**, etc...
- **Renderer Plugin System** — Users will be able to choose or write their own rendering backend.
- **ECS Framework** — Replace the current simple entity system with a proper Entity-Component-System architecture.
- **Tooling** — Editor, asset pipeline, and profiling tools.

---

### Acknowledgements

Atom Engine uses the following open-source libraries. We extend our sincere gratitude to their developers:

- [SFML](https://www.sfml-dev.org/) — Windowing, rendering, audio, and input
- [ImGui](https://github.com/ocornut/imgui) — Debug overlay UI
- [ImGui-SFML](https://github.com/SFML/imgui-sfml) — ImGui integration for SFML
- [Lua](https://www.lua.org/) — Scripting engine
- [TagLib](https://taglib.org/) — Audio metadata reading
- [utfcpp](https://github.com/nemtrif/utfcpp) — UTF-8 validation and conversion
- [FFmpeg](https://ffmpeg.org/) — Video and audio decoding

---

## License
This Project is licensed under the [MIT License](LICENSE) 


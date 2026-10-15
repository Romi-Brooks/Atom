# Atom Engine — Coding Standard
[English](CODING_STANDARD.md) | [中文](Docs/CODING_STANDARD-CN.md)  

---
> This is a quick-reference guide to the project architecture and code style followed during Atom Engine development. Please adhere to this guide when contributing code to Atom Engine.

The repository `.clang-format` file is the formatting source of truth. Run
`clang-format -i <files>` on modified C/C++ files before submitting changes.

---

## 1. Directory and File Naming

### 1.1 Directory Structure

```
ModuleName/            # PascalCase, singular
├── Manager/           # Manager sub-module for this module
├── Plug/              # Plugin/extension sub-module
├── SubModule/         # Sub-directory split by functionality
│   ├── Foo.cpp
│   └── Foo.hpp
├── ModuleName.cpp     # Main source file
├── ModuleName.hpp     # Main header file
└── CMakeLists.txt     # (Optional) Sub-CMake configuration
```

**Rules:**

- Directory names match module names — use **PascalCase**
- Sub-directories are grouped by function/responsibility, not by file type (avoid `src/`, `include/`, `headers/`-like structures)
- `Manager/`, `Plug/` etc. reflect responsibility

### 1.2 File Naming

| File Type | Convention | Example |
|---|---|---|
| Class definition header | Exactly matches class name, PascalCase | `Entity.hpp`, `VolumeManager.hpp` |
| Class implementation source | Same name as header | `Entity.cpp`, `VolumeManager.cpp` |
| Non-class utility files | Descriptive PascalCase | `LogSystem.hpp`, `MusicFade.hpp` |
| Test files | `*Test.cpp` | `AudioPlaybackTest.cpp` |

---

## 2. Header File Rules

### 2.1 Include Guard

```cpp
#ifndef ATOM_MODULENAME_HPP
#define ATOM_MODULENAME_HPP
// ...
#endif // ATOM_MODULENAME_HPP
```

**Rules:**

- Format: `ATOM_<NAME>_HPP`, `<NAME>` is PascalCase in all caps
- `<NAME>` is the **file name** (`ScreenManager.hpp` → `ATOM_SCREENMANAGER_HPP`). A directory
  component may be kept in front of it (`Backend/Contracts/Window/IWindow.hpp` →
  `ATOM_BACKEND_CONTRACTS_WINDOW_IWINDOW_HPP`); dropping a leading `I` from an interface name is
  accepted (`IAudioEffect.hpp` → `ATOM_AUDIO_EFFECT_HPP`). The guard must never be a stale name
  from a previous file name — it is the only thing that tells a reader which file the block
  belongs to.
- `#ifndef` and `#define` are adjacent, with nothing between them
- `#endif` must be followed by a comment with the macro name
- `#pragma once` is not used anywhere in this repository

### 2.2 Include Order

In an implementation file, include its corresponding header first so that
missing self-contained dependencies are detected immediately. Group all other
headers in the following order, separated by blank lines:

```cpp
// Self Dependency
#include "VolumeManager.hpp"

// Standard Library
#include <memory>
#include <string>
#include <unordered_map>

// Third Party Library
#include <SDL3/SDL.h>

// Engine Headers
#include <Media/Audio/Music.hpp>
#include <Log/LogSystem.hpp>
```

**Rules:**

- In `.cpp` files: **Self Dependency (`""`)** → **Standard Library** → **Third Party** → **Project Headers (`<>`)**
- Header files omit the self dependency and start with standard-library includes
- Project headers use `#include <ModuleName/FileName.hpp>` syntax (relative to the project root)
- Self (corresponding `.hpp`) uses `#include "FileName.hpp"`, placed first
- Relative include paths (`../`, `../../`) are **prohibited**: always spell an engine header from
  the project root
- Include what you use: a file includes the header that declares each name it uses, and no file
  includes the same header twice
- The only exception to the grouping above is a single forward-declaration block, which belongs
  with the project headers

### 2.3 `using` and Type Aliases

- A public header must **not** write `using namespace xxx;` — it pollutes the lookup scope of
  every file that includes it
- `using Name = Type;` declares a scoped alias in the current namespace and does not import the
  name anywhere else; it is allowed for a deliberate API facade, a resource handle, or an in-class
  callback type
- A namespace-scope alias must denote **exactly the same type and unit semantics** and live in the
  namespace that owns the API (e.g. `render::Color = color::Color`). If the coordinate system,
  units, ownership or invariants differ, use a distinct struct/wrapper instead of an alias
- An alias cannot be forward-declared on its own. If a public interface stores or constructs the
  type by value, the header must include the underlying type's complete definition; when only
  pointers or references are used, the underlying tag may be forward-declared before the alias
- Do not add namespace-scope aliases just to shorten local code; solve a name clash with qualified
  namespaces or a clearer type name. A new public alias must state its stable-API intent in review

---

## 3. Naming Conventions

### 3.1 Quick Reference

| Category | Style | Example |
|---|---|---|
| **Namespace** | `snake_case` | `atom`, `atom::audio` |
| **Class / Struct** | `PascalCase` | `VolumeManager`, `MusicFade`, `SFX` |
| **Enum type** | `PascalCase` | `LogLevel`, `FadeState`, `NPCType` |
| **Enum values** | `PascalCase` | `Idle`, `FadingOut`, `ErrorInvalidFormat` |
| **Constants** (`const` / `constexpr`, any scope) | `snake_case` | `max_volume`, `default_recent_capacity`, `invalid_texture_2d` |
| **Public / Private member functions** | `PascalCase` | `GetInstance()`, `Load()`, `SetVolume()` |
| **Static member functions** | `PascalCase` | `GetInstance()`, `SetSfxVolume()` |
| **Member variables** | `snake_case_` + trailing underscore | `music_volume_`, `current_playing_id_` |
| **Static member variables** | `snake_case_` + trailing underscore | `static float music_volume_` |
| **Public aggregate/config fields** | `snake_case` | `sample_rate`, `output_dir` |
| **Function parameters** | `snake_case` | `id`, `file_path`, `target` |
| **Local variables** | `snake_case` | `it`, `load_result`, `music` |
| **Macros** | `UPPER_SNAKE_CASE` | `LOG_INFO`, `LOG_ERROR` |

### 3.1.1 Log Channel Naming

Channels are enum-backed values grouped below the logging namespace — each
domain is one `ATOM_DEFINE_CHANNELS` block (engine channels live in
`Log/AtomLogChannels.hpp`, game domains live in the game project). A domain owns
a namespace, an internal `Channel` enum and a display prefix:

| Domain | Prefix | Example usage |
|---|---|---|
| `atom::log::core` | `Atom.` | `atom::log::core::Main` |
| `atom::log::audio` | `Atom.Audio.` | `atom::log::audio::Music` |
| `atom::log::render` | `Atom.Render.` | `atom::log::render::Renderer2D` |
| `atom::log::image` | `Atom.Image.` | `atom::log::image::Decoder` |
| `atom::log::backend::Audio` | `Atom.Backend.Audio.` | `atom::log::backend::Audio::sdl3` |
| `game::log` | `Game.` | `game::log::Npc` |

- Channel values are `PascalCase` (`ScreenManager`, `PlugMusicFade`). Backend
  implementation identifiers are the exception: use their canonical registry
  IDs (`sdl3`, `sdl3_mixer`) so log channels and runtime backend IDs match.
- Display names are dotted PascalCase components (`Atom.Entity.NPC ->`).
- Always reference a channel through its logging domain at the call site
  (e.g. `atom::log::audio::Music`). Do not introduce local aliases such as
  `const auto& music_log_channel = atom::log::audio::Music;` — they add
  indirection for readers without meaningful benefit.

### 3.2 Detailed Rules

#### Namespace

- Use `snake_case`
- Top-level namespace: `atom`
- Sub-namespaces: `atom::audio`, `atom::video` (if further subdivision is needed)
- `using namespace` is **forbidden in headers**; a `.cpp` may use it only for its own module's
  namespace (or in a test), never to pull in a third-party namespace

#### Classes

- PascalCase, starting with an uppercase letter
- Abbreviations in all caps: e.g., `SFXManager`
- New class names should use full words or well-known abbreviations

#### Functions

- Engine C++ functions use **trailing return types**: `auto FuncName() -> ReturnType`
- Constructors, destructors, `main`, and callbacks whose signatures must match a C API are exempt
  (the `int LuaXxx(lua_State* L)` binding functions in `Lua/LuaBinding/` are the only in-tree case)
- The rule also covers small in-class helpers: `auto Reset() -> void`, not `void Reset()`
- PascalCase, starting with a verb: `GetInstance()`, `Load()`, `SetVolume()`
- Getters start with `Get`, setters start with `Set`
- Boolean queries start with `Is` / `Has`: `IsLoaded()`, `HasSFX()`
- Constructors/destructors use traditional syntax

```cpp
// Correct
auto Play(const std::string& id) -> void;
auto GetMusicVolume() const -> float;
[[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;

// Avoid
void Play(const std::string& id);
```

#### Member Variables

- `snake_case_` — **trailing underscore is required**
- Do not use `m_` prefix or leading underscore
- Public fields in simple aggregate/configuration structs use `snake_case`

```cpp
class Music {
    private:
        std::unordered_map<std::string, std::unique_ptr<IAudioSource>> tracks_;
        std::string current_playing_id_;
        static float music_volume_;
};
```

#### Constants

- `snake_case` at every scope: namespace scope, class scope, file scope and
  function scope
- **No `k` prefix** and **no trailing underscore**. The trailing underscore marks
  object state, and a constant is not object state; a class-scope constant is
  already distinguished by the qualification at its use site
  (`Log::default_recent_capacity`, `LayoutTree::invalid_node`)
- `UPPER_SNAKE_CASE` stays reserved for macros — do not use it for a C++
  constant
- A generated constant that must be spelled through a macro cannot be
  lowercased by the preprocessor, so the macro emits a fixed `snake_case` name
  and relies on the one-domain-per-namespace rule above
  (`LogChannelMacros.hpp`: `channel_names`, `channel_registered`)

```cpp
namespace atom::numeric {
inline constexpr float max_volume = 100.0f;  // namespace scope
}

class Log {
    public:
        static constexpr std::size_t default_recent_capacity = 512;  // class scope
};

namespace {
constexpr float two_pi = 6.28318530717958647692f;  // file scope
}
```

#### Parameters and Local Variables

- `snake_case`, starting with a lowercase letter
- Single-letter variables are limited to loop counters (`i`) or iterators (`it`)
- Boolean parameters use verbs or adjectives: `is_enabled`, `should_loop`

#### Private Types Inside Classes

- Enum type name: PascalCase
- Enum values: PascalCase
- If an enum or struct is only used within the class, define it in the `private:` section:

```cpp
class MusicFade {
    private:
        enum class FadeState {
            Idle,
            FadingOut,
            FadingIn,
            Completed
        };

        struct FadeContext {
            std::string from_id;
            std::string to_id;
            float duration{0.0f};
            FadeState state{FadeState::Idle};
        } context_;
};
```

#### File Name Matches Class Name

- One file per class; the file name must match the primary class name

---

## 4. Code Formatting

### 4.1 Indentation and Braces

- **Indentation:** 4 spaces; tabs are not used for indentation
- **Brace style:** K&R (opening brace on the declaration or control-statement line)

```cpp
namespace atom {
class Entity {
    public:
        auto GetHP() const -> float {
            return hp_;
        }

    private:
        float hp_;
};
}
```

### 4.2 Access Control Order

```cpp
class ClassName {
    public:
        ClassName() = default;
        ~ClassName() = default;

        ClassName(const ClassName&) = delete;
        auto operator=(const ClassName&) -> ClassName& = delete;

        static auto GetInstance() -> ClassName&;
        auto DoSomething() -> void;

    private:
        auto Helper() -> void;

        int member_;
};
```

**Rules:**

- Prefer `public:` → `protected:` → `private:` so the public API is visible first
- Access specifiers are indented one level inside the class; declarations are indented one additional level
- Keep member initialization order in mind when moving declarations; style-only changes must not alter behavior

### 4.3 Member Initialization

- Use uniform initialization (brace-init) as the default

```cpp
float moveSpeed_ {};           // value-initialization
float music_volume_ = 100.0f;  // = is also acceptable
unsigned int fps_ = 60;
```

### 4.4 Pointers and References

```cpp
auto DoSomething(const std::string& str) -> void;
auto DoSomething(std::string&& str) -> void;
auto GetPointer() const -> SomeType*;
```

- `*` and `&` are attached to the type (left-aligned), not to the variable name

### 4.5 `const` Position

- Member function `const` is **trailing**:

```cpp
auto GetValue() const -> float;       // correct
auto GetValue()const -> float;        // missing space
```

---

## 5. C++23 Feature Usage

### 5.1 Mandatory

| Feature | Usage |
|---|---|
| **Trailing return types** | `auto Func() -> Type` for engine C++ functions; C callbacks and `main` are exempt |
| **`auto` placeholder** | `auto it = map.find(id);` |
| **`[[nodiscard]]`** | All getters, query functions, and functions whose return value should not be ignored |
| **`enum class`** | All enums must use `enum class`; bare `enum` is forbidden |
| **Range-for + structured bindings** | `for (const auto& [key, val] : map)` |

### 5.2 Recommended (future support)

| Feature | Usage |
|---|---|
| **`std::expected`** | For fallible return values; the engine standardizes on `atom::Result<T>` (see §6) |
| **`std::print` / `std::format`** | Replace `std::cout` / string concatenation |
| **`consteval` / `constexpr`** | Compile-time evaluated functions |
| **`std::span`** | Replace `const std::vector<T>&` parameters |
| **`std::optional`** | Replace nullable return values |
| **`std::views`** | Use `std::views::filter`, `std::views::transform`, etc. |

---

## 6. Unified Error Rules (CORE-004)

The engine has exactly **one** failure representation: `atom::Result<T>` (`Error/Error.hpp`). Every new or
modified fallible API follows this section.

### 6.1 What to return

| Case | Return type |
|---|---|
| Can fail and carries a value | `atom::Result<T>` |
| Can fail and carries no value | `atom::Result<void>` |
| Only success/failure, and the reason does not need reporting (internal branching) | `bool`, with the function named `Try*` / `Is*` / `Has*` |
| Genuinely cannot fail | `void`, with a comment saying so |

- Do **not** add module-specific result enums. The existing ones (`fs::Result`, `AssetResult`,
  `DecoderOpenStatus`, `ScreenOpResult`, `PackageBuilder::Result`, ...) survive only as small backend-boundary
  codes: hot paths (`ReadAt`, `ReadNext`) return them, and the layer above converts once with `ToError()`.
  A `std::string` must not travel through a read path.
- Do **not** use exceptions for expected failures (missing file, unsupported format, device open failure).
  Exceptions are for programming errors.
- Do **not** "log and return `void`/`bool`". The caller must be able to obtain the reason.
- A function returning `Result` still needs `[[nodiscard]]`; it does not propagate from the return type.

### 6.2 Error codes

- Append inside the block of the module that **owns the decision** (256 values per module in `ErrorCode`).
  Cross-module translation is only for carrying a lower layer's reason upward (asset surfacing `FsNotFound`);
  that is legal and visible.
- **Append only.** Never renumber an existing enumerator, never reuse a retired value.
- Use `ErrorCodeName()` and `ModuleName()`. Do not write another switch over codes somewhere else.

### 6.3 Context and underlying cause

```cpp
// At the failure site: fill in the reason once.
const auto opened = filesystem.OpenRead(path, file);
if (opened != fs::Result::Success)
    return std::unexpected<Error>{fs::ToError(opened, "OpenRead", path.String())};
```

- `operation` names what the **innermost** layer was doing. `WithOperation()` never overwrites an existing
  value, so "OpenRead failed" cannot be buried under an upper layer's "AcquireRecord failed".
- `resource` is an `AssetPath` string (`Error` does not depend on Filesystem, so it is a string).
- **Capture platform error state at the failure site**: `SDL_GetError`, `GetLastError` and `errno` are single
  slots overwritten by the next call. Use `CaptureSdlError()` (reads and clears), `CaptureLastSystemError()`,
  `CaptureErrnoMessage()`.

### 6.4 Who logs

- `Error` never logs, and provides no "log and return" helper: `Log` depends on `Error`, and the reverse would
  break the leaf-module layering.
- Log **once**, in the layer closest to the failure that knows the operation and the resource; other layers
  only forward:

```cpp
LOG_ERROR(atom::log::asset::Resource, "ResourceManager: " + error.Message());
```

- One failure must produce exactly **one** diagnostic. Forwarding layers do not log again.
- Use the module's own log channel; do not borrow an unrelated one such as `core::Screen`.

### 6.5 Boundaries of `Error`

- `Error` is a plain value: no exceptions, no dependency on Log or a backend, no allocation on success paths.
- A default-constructed `Error` means `Internal`, **not** success: presence of failure is carried by `Result`.
- To add a cross-module failure category, add it to the module's `ErrorCode` block and convert at the use
  site. Do not introduce another enum.

---

## 7. Comment Rules

Comments are written in English by default. Use another language (such as
Chinese) only when necessary — for example, to quote a localized string or
clarify a concept that English cannot express faithfully.

### 6.1 Doxygen File Header

```cpp
/**
  * @file           : FileName.hpp
  * @author         : Author
  * @brief          : One-line description.
  * @attention      : Optional caveats or notes.
  * @date           : YYYY/M/D
  Copyright (c) YYYY Author, All rights reserved.
**/
```

This is the current header format for new Atom files and examples. Every file
added from now on carries it. Existing headers are not being migrated in this
phase; the SPDX/license header migration is tracked as **CORE-007** in
`Docs/Remaining-Issues.md` (current audit: 161 Atom-owned C/C++ files, 45 with
the old header, 9 with SPDX). Do not bolt a second header style onto a file that
is being edited for another reason — leave the migration to CORE-007.

### 6.2 Inline Comments

```cpp
// Get current SFX volume
auto GetSfxVolume() const -> float;
```

### 6.3 Complex Method / Property Comments

```cpp
/**
  * @name           : Method/Property
  * @author         : Author
  * @brief          : Description for the method/property
  * @attention      : Any caveats or notes
**/
```

### 6.4 `TODO` / `FIXME`

```cpp
// TODO(Author): Implement retry logic
// FIXME: Race condition on shutdown
```

# Atom Engine — Coding Standard
[English](CODING_STANDARD.md) | [中文](Docs/CODING_STANDARD-CN.md)  

---
> This is a quick-reference guide to the project architecture and code style followed during Atom Engine development. Please adhere to this guide when contributing code to Atom Engine.

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
- `#endif` must be followed by a comment with the macro name

### 2.2 Include Order

Group headers in the following order, separated by blank lines:

```cpp
// Standard Library
#include <memory>
#include <string>
#include <unordered_map>

// Third Party Library
#include <SFML/Audio/Music.hpp>

// Engine Headers
#include <Media/Audio/Music.hpp>
#include <Log/LogSystem.hpp>

// Self Dependency
#include "VolumeManager.hpp"
```

**Rules:**

- **Standard Library** → **Third Party** → **Project Headers (`<>`)** → **Self Dependency (`""`)**
- Project headers use `#include <ModuleName/FileName.hpp>` syntax (relative to the project root)
- Self (corresponding `.hpp`) uses `#include "FileName.hpp"`, placed at the very end
- Relative paths like `../../` are prohibited

---

## 3. Naming Conventions

### 3.1 Quick Reference

| Category | Style | Example |
|---|---|---|
| **Namespace** | `snake_case` | `atom`, `atom::audio` |
| **Class / Struct** | `PascalCase` | `VolumeManager`, `MusicFade`, `SFX` |
| **Enum type** | `PascalCase` | `LogLevel`, `FadeState`, `NPCType` |
| **Enum values** | `PascalCase` | `Idle`, `FadingOut` |
| **Public / Private member functions** | `PascalCase` | `GetInstance()`, `Load()`, `SetVolume()` |
| **Static member functions** | `PascalCase` | `GetInstance()`, `SetSfxVolume()` |
| **Member variables** | `snake_case_` + trailing underscore | `music_volume_`, `current_playing_id_` |
| **Static member variables** | `snake_case_` + trailing underscore | `static float music_volume_` |
| **Function parameters** | `camelCase` | `id`, `filePath`, `target` |
| **Local variables** | `camelCase` | `it`, `result`, `music` |
| **Macros** | `UPPER_SNAKE_CASE` | `LOG_INFO`, `LOG_ERROR` |

### 3.2 Detailed Rules

#### Namespace

- Use `snake_case`
- Top-level namespace: `atom`
- Sub-namespaces: `atom::audio`, `atom::video` (if further subdivision is needed)

#### Classes

- PascalCase, starting with an uppercase letter
- Abbreviations in all caps: e.g., `SFXManager`
- New class names should use full words or well-known abbreviations

#### Functions

- **All non-trivial functions** use **trailing return type**: `auto FuncName() -> ReturnType`
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

```cpp
class Music {
    private:
        std::unordered_map<std::string, std::unique_ptr<sf::Music>> musics_;
        std::string current_playing_id_;
        static float music_volume_;
};
```

#### Parameters and Local Variables

- `camelCase`, starting with a lowercase letter
- Single-letter variables are limited to loop counters (`i`) or iterators (`it`)
- Boolean parameters use verbs or adjectives: `isEnabled`, `shouldLoop`

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
            std::string fromId;
            std::string toId;
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

- **Indentation:** Tab (1 Tab = 4 character width)
- **Brace style:** Allman (braces on their own line)

```cpp
namespace atom {
    class Entity {
        private:
            float hp_;

        public:
            auto GetHP() const -> float
            {
                return hp_;
            }
    };
}
```

### 4.2 Access Control Order

```cpp
class ClassName {
    private:
        // Member variables first
        int member_;

    public:
        // Constructor / Destructor
        ClassName() = default;
        ~ClassName() = default;

        // Delete copy
        ClassName(const ClassName&) = delete;
        auto operator=(const ClassName&) -> ClassName& = delete;

        // Static methods
        static ClassName& GetInstance();

        // Public methods
        auto DoSomething() -> void;

    private:
        // Private methods
        auto Helper() -> void;
};
```

**Rules:**

- `private:` → `public:` order (member variables first, then functions)
- Each access specifier is indented one level; its contents are indented one more level

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
| **Trailing return types** | `auto Func() -> Type` (required for all non-constructor/destructor functions) |
| **`auto` placeholder** | `auto it = map.find(id);` |
| **`[[nodiscard]]`** | All getters, query functions, and functions whose return value should not be ignored |
| **`enum class`** | All enums must use `enum class`; bare `enum` is forbidden |
| **Range-for + structured bindings** | `for (const auto& [key, val] : map)` |

### 5.2 Recommended (future support)

| Feature | Usage |
|---|---|
| **`std::expected`** | For fallible return values (if compiler supports it) |
| **`std::print` / `std::format`** | Replace `std::cout` / string concatenation |
| **`consteval` / `constexpr`** | Compile-time evaluated functions |
| **`std::span`** | Replace `const std::vector<T>&` parameters |
| **`std::optional`** | Replace nullable return values |
| **`std::views`** | Use `std::views::filter`, `std::views::transform`, etc. |

---

## 6. Comment Rules

### 6.1 Doxygen File Header

```cpp
/**
  * @file           : FileName.hpp
  * @author         : Author
  * @brief          : One-line description
  * @attention      : Any caveats or notes
  * @date           : YYYY/MM/DD
  Copyright (c) 2025 Author,  All rights reserved.
**/
```

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

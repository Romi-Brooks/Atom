# Third-Party Dependencies

Atom tracks third-party source code as pinned Git submodules. CMake builds these
dependencies through [`ThirdParty/CMakeLists.txt`](CMakeLists.txt) with the same
toolchain used for the engine.

| Dependency | Usage | Git URL | Locked commit |
|---|---|---|---|
| SDL3 | Windowing, rendering, audio, and input backend | `https://github.com/libsdl-org/SDL.git` | `f87239e71e42da91ca317a12eefb82cfbf3393eb` (`release-3.4.12`) |
| TagLib | Audio metadata reading | `https://github.com/taglib/taglib.git` | `7d86716194777e0294453bfdc9dd170bd033e1f4` (`v2.1.1`) |
| Dear ImGui | Debug overlay UI with SDL3 backends | `https://github.com/ocornut/imgui.git` | `01380c579715e62fb9a8d6ec0502c4ea83bfde6e` (`v1.92.9`) |
| Lua | Embedded scripting runtime | `https://github.com/lua/lua.git` | `1ab3208a1fceb12fca8f24ba57d6e13c5bff15e3` (`v5.4.7`) |
| utfcpp | UTF-8 validation and conversion | `https://github.com/nemtrif/utfcpp.git` | `f9319195dfddf369f68f18e7c0039b3f351797fd` (`v4.0.8`) |

Initialize the versions locked by Atom:

```bash
git submodule sync --recursive
git submodule update --init
```

Do not use `git submodule update --remote` in a normal build: it follows
upstream branches instead of the commits tested by Atom. Dependency upgrades
must update the submodule pointer, this table, and any affected CMake rules in
the same change.

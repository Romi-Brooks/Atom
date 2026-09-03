# Third-Party Dependencies

Atom tracks most third-party source code as pinned Git submodules. Small
single-header dependencies may be vendored with their version, provenance,
license and integrity hashes recorded in their own directory. CMake exposes
all dependencies through [`ThirdParty/CMakeLists.txt`](CMakeLists.txt) with the
same toolchain used for the engine.

When an upstream project hardcodes a non-Atom target name, Atom creates a
CMake-only copy under the build directory and renames the target there. The
pinned submodule remains untouched; see `TargetOverlay.cmake`.

| Dependency | Atom target | Usage | Git URL | Locked commit |
|---|---|---|---|---|
| SDL3 | `Atom_3rd_SDL3` | Windowing, rendering, audio, and input backend | `https://github.com/libsdl-org/SDL.git` | `f87239e71e42da91ca317a12eefb82cfbf3393eb` (`release-3.4.12`) |
| TagLib | `Atom_3rd_TagLib` | Audio metadata reading | `https://github.com/taglib/taglib.git` | `7d86716194777e0294453bfdc9dd170bd033e1f4` (`v2.1.1`) |
| Dear ImGui | `Atom_3rd_ImGui`, `Atom_3rd_ImGui_SDL3` | Debug overlay UI with SDL3 backends | `https://github.com/ocornut/imgui.git` | `01380c579715e62fb9a8d6ec0502c4ea83bfde6e` (`v1.92.9`) |
| Lua | `Atom_3rd_Lua` | Embedded scripting runtime | `https://github.com/lua/lua.git` | `1ab3208a1fceb12fca8f24ba57d6e13c5bff15e3` (`v5.4.7`) |
| utfcpp | `Atom_3rd_UtfCpp` | UTF-8 validation and conversion | `https://github.com/nemtrif/utfcpp.git` | `f9319195dfddf369f68f18e7c0039b3f351797fd` (`v4.0.8`) |
| minimp3 | `Atom_3rd_Minimp3` | MP3 decoding | `https://github.com/lieff/minimp3.git` | `ea99364f61c14656440e8d77e9c233ccf3124633` |
| Yoga | `Atom_3rd_Yoga` | Flexbox-compatible UI layout calculation | `https://github.com/facebook/yoga.git` | `042f5013152eb81c1552dec945b88f7b95ca350f` (`v3.2.1`) |
| stb | `Atom_3rd_Stb` | Image decoding and trusted font rasterization | vendored from `https://github.com/nothings/stb` | `stb_image` v2.30, `stb_truetype` v1.26; see [`stb/README.md`](stb/README.md) |

Initialize the versions locked by Atom:

```bash
git submodule sync --recursive
git submodule update --init
```

Do not use `git submodule update --remote` in a normal build: it follows
upstream branches instead of the commits tested by Atom. Dependency upgrades
must update the submodule pointer, this table, and any affected CMake rules in
the same change.

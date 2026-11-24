/**
 * @file           : LuaContext.hpp
 * @brief          : Central injection point for engine objects exposed to Lua.
 * @attention      : Replaces the per-binding global pointer bridge (g_music,
 *                   g_sfx, g_mixer, g_transition, g_filesystem). Bindings read
 *                   their dependencies from this single struct instead of
 *                   holding hidden module-local state.
**/

#ifndef ATOM_LUA_CONTEXT_HPP
#define ATOM_LUA_CONTEXT_HPP

namespace atom {
class MusicPlayer;
class SFXPlayer;
class AudioMixer;

namespace fs {
class Vfs;
}

namespace audio {
class MusicCrossfade;
}

// All engine objects a Lua binding may need, gathered in one place and owned by
// the application. A null pointer means "that subsystem is not attached": a
// binding must report a Lua error instead of dereferencing null.
struct LuaContext {
        // Filesystem used by string entry points. Null means "use the
        // process-wide default Vfs::GetInstance()".
        atom::fs::Vfs* vfs = nullptr;
        atom::MusicPlayer* music = nullptr;
        atom::SFXPlayer* sfx = nullptr;
        atom::AudioMixer* mixer = nullptr;
        atom::audio::MusicCrossfade* crossfade = nullptr;
};

} // namespace atom

#endif // ATOM_LUA_CONTEXT_HPP

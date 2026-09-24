/**
 * @file           : LuaVolume.cpp
 * @brief          : AudioMixer bindings under the Atom.Audio.Mixer table.
**/

#include "ContextBridge.hpp"
#include "lua.hpp"

#include <Log/LogSystem.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>

namespace {

auto Mixer(const atom::LuaContext& context, lua_State* L) -> atom::AudioMixer& {
    if (!context.mixer)
        luaL_error(L, "AudioMixer is not attached to the Lua context");
    return *context.mixer;
}

int LuaMixerSetMasterVolume(lua_State* L) {
    Mixer(*atom::GetLuaContext(L), L).SetMasterVolume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

int LuaMixerGetMasterVolume(lua_State* L) {
    lua_pushnumber(L, Mixer(*atom::GetLuaContext(L), L).GetMasterVolume());
    return 1;
}

int LuaMixerSetSfxVolume(lua_State* L) {
    Mixer(*atom::GetLuaContext(L), L).SetSFXVolume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

int LuaMixerGetSfxVolume(lua_State* L) {
    lua_pushnumber(L, Mixer(*atom::GetLuaContext(L), L).GetSFXVolume());
    return 1;
}

int LuaMixerSetMusicVolume(lua_State* L) {
    Mixer(*atom::GetLuaContext(L), L).SetMusicVolume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

int LuaMixerGetMusicVolume(lua_State* L) {
    lua_pushnumber(L, Mixer(*atom::GetLuaContext(L), L).GetMusicVolume());
    return 1;
}

} // namespace

namespace atom {

auto RegisterVolumeToLua(lua_State* L, LuaContext& context) -> void {
    const int top = lua_gettop(L);
    PushNamespace(L, {"Atom", "Audio"});
    lua_newtable(L);

    const luaL_Reg functions[] = {{"SetMasterVolume", LuaMixerSetMasterVolume},
                                  {"GetMasterVolume", LuaMixerGetMasterVolume},
                                  {"SetSfxVolume", LuaMixerSetSfxVolume},
                                  {"GetSfxVolume", LuaMixerGetSfxVolume},
                                  {"SetMusicVolume", LuaMixerSetMusicVolume},
                                  {"GetMusicVolume", LuaMixerGetMusicVolume},
                                  {nullptr, nullptr}};
    luaL_setfuncs(L, functions, 0);

    lua_setfield(L, -2, "Mixer"); // Atom.Audio.Mixer = functions
    lua_settop(L, top);           // restore stack balance across registrations
    LOG_INFO(atom::log::core::Lua, "Bound Atom.Audio.Mixer");
}

} // namespace atom

#include "lua.hpp"
#include <Media/Audio/Mixing/AudioMixer.hpp>
namespace {
atom::AudioMixer* g_mixer = nullptr;
}
auto SetLuaAudioMixerInstance(atom::AudioMixer& mixer) -> void {
    g_mixer = &mixer;
}
static auto RequireMixer(lua_State* L) -> atom::AudioMixer& {
    if (!g_mixer)
        luaL_error(L, "AudioMixer is not attached");
    return *g_mixer;
}
static int lua_Volume_SetMasterVolume(lua_State* L) {
    RequireMixer(L).SetMasterVolume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}
static int lua_Volume_GetMasterVolume(lua_State* L) {
    lua_pushnumber(L, RequireMixer(L).GetMasterVolume());
    return 1;
}
static int lua_Volume_SetSfxVolume(lua_State* L) {
    RequireMixer(L).SetSFXVolume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}
static int lua_Volume_GetSfxVolume(lua_State* L) {
    lua_pushnumber(L, RequireMixer(L).GetSFXVolume());
    return 1;
}
static int lua_Volume_SetMusicVolume(lua_State* L) {
    RequireMixer(L).SetMusicVolume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}
static int lua_Volume_GetMusicVolume(lua_State* L) {
    lua_pushnumber(L, RequireMixer(L).GetMusicVolume());
    return 1;
}
auto RegisterVolumeToLua(lua_State* L) -> void {
    lua_newtable(L);
    const luaL_Reg functions[] = {{"SetMasterVolume", lua_Volume_SetMasterVolume},
                                  {"GetMasterVolume", lua_Volume_GetMasterVolume},
                                  {"SetSfxVolume", lua_Volume_SetSfxVolume},
                                  {"GetSfxVolume", lua_Volume_GetSfxVolume},
                                  {"SetMusicVolume", lua_Volume_SetMusicVolume},
                                  {"GetMusicVolume", lua_Volume_GetMusicVolume},
                                  {nullptr, nullptr}};
    luaL_setfuncs(L, functions, 0);
    lua_setglobal(L, "AudioMixer");
}
/**
 * @file           : LuaMusic.cpp
 * @brief          : Music and crossfade bindings under the Atom.Audio.Music table.
 * @attention      : Load only accepts a res:// asset-path string; it is parsed
 *                   and resolved through the process-wide default Vfs.
**/

#include "ContextBridge.hpp"
#include "lua.hpp"

#include <Log/LogSystem.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Audio/Transitions/MusicCrossfade.hpp>

namespace {

auto Music(const atom::LuaContext& context, lua_State* L) -> atom::MusicPlayer& {
    if (!context.music)
        luaL_error(L, "MusicPlayer is not attached to the Lua context");
    return *context.music;
}

auto Crossfade(const atom::LuaContext& context, lua_State* L) -> atom::audio::MusicCrossfade& {
    if (!context.crossfade)
        luaL_error(L, "MusicCrossfade is not attached to the Lua context");
    return *context.crossfade;
}

int LuaMusicLoad(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    lua_pushboolean(L, Music(context, L).Load(luaL_checkstring(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

int LuaMusicPlay(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    Music(context, L).Play(luaL_checkstring(L, 1), static_cast<float>(luaL_optnumber(L, 2, 100.0)));
    return 0;
}

int LuaMusicStop(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    Music(context, L).Stop(luaL_checkstring(L, 1));
    return 0;
}

int LuaMusicSetVolume(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    Music(context, L).SetVolume(luaL_checkstring(L, 1), static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

int LuaMusicIsLoaded(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    lua_pushboolean(L, Music(context, L).IsLoaded(luaL_checkstring(L, 1)));
    return 1;
}

int LuaMusicCrossfade(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    lua_pushboolean(L, Crossfade(context, L).Switch(luaL_checkstring(L, 1),
                                                    static_cast<float>(luaL_optnumber(L, 2, 2.0))));
    return 1;
}

int LuaMusicCancelTransition(lua_State* L) {
    Crossfade(*atom::GetLuaContext(L), L).Cancel();
    return 0;
}

int LuaMusicGetTransitionProgress(lua_State* L) {
    lua_pushnumber(L, Crossfade(*atom::GetLuaContext(L), L).GetProgress());
    return 1;
}

} // namespace

namespace atom {

auto RegisterMusicToLua(lua_State* L, LuaContext& context) -> void {
    const int top = lua_gettop(L);
    PushNamespace(L, {"Atom", "Audio"});
    lua_newtable(L);

    const luaL_Reg functions[] = {{"Load", LuaMusicLoad},
                                  {"Play", LuaMusicPlay},
                                  {"Stop", LuaMusicStop},
                                  {"SetVolume", LuaMusicSetVolume},
                                  {"IsLoaded", LuaMusicIsLoaded},
                                  {"Crossfade", LuaMusicCrossfade},
                                  {"CancelTransition", LuaMusicCancelTransition},
                                  {"GetTransitionProgress", LuaMusicGetTransitionProgress},
                                  {nullptr, nullptr}};
    luaL_setfuncs(L, functions, 0);

    lua_setfield(L, -2, "Music"); // Atom.Audio.Music = functions
    lua_settop(L, top);           // restore stack balance across registrations
    LOG_INFO(atom::log::core::Lua, "Bound Atom.Audio.Music");
}

} // namespace atom

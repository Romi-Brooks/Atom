/**
 * @file           : LuaSFX.cpp
 * @brief          : SFX bindings under the Atom.Audio.SFX table.
 * @attention      : Load only accepts a res:// asset-path string, resolved
 *                   through the process-wide default Vfs. The previous split
 *                   between "SFX" and "AudioClipCache" tables (two names for the
 *                   same Load) is collapsed into a single surface.
**/

#include "ContextBridge.hpp"
#include "lua.hpp"

#include <Log/LogSystem.hpp>
#include <Media/Audio/Playback/SFXPlayer.hpp>

namespace {

auto Sfx(const atom::LuaContext& context, lua_State* L) -> atom::SFXPlayer& {
    if (!context.sfx)
        luaL_error(L, "SFXPlayer is not attached to the Lua context");
    return *context.sfx;
}

int LuaSfxLoad(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    lua_pushboolean(L, Sfx(context, L).Load(luaL_checkstring(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

int LuaSfxPlay(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    const char* id = luaL_checkstring(L, 1);
    if (lua_isnumber(L, 2))
        Sfx(context, L).Play(id, static_cast<float>(lua_tonumber(L, 2)));
    else
        Sfx(context, L).Play(id);
    return 0;
}

int LuaSfxStop(lua_State* L) {
    Sfx(*atom::GetLuaContext(L), L).Stop(luaL_checkstring(L, 1));
    return 0;
}

int LuaSfxStopAll(lua_State* L) {
    Sfx(*atom::GetLuaContext(L), L).StopAll();
    return 0;
}

int LuaSfxSetVolume(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    Sfx(context, L).SetVolume(luaL_checkstring(L, 1), static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

int LuaSfxIsLoaded(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    lua_pushboolean(L, Sfx(context, L).IsLoaded(luaL_checkstring(L, 1)));
    return 1;
}

int LuaSfxUnload(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    lua_pushboolean(L, Sfx(context, L).Unload(luaL_checkstring(L, 1)));
    return 1;
}

int LuaSfxReset(lua_State* L) {
    Sfx(*atom::GetLuaContext(L), L).Reset();
    return 0;
}

int LuaSfxGetLoadedCount(lua_State* L) {
    const auto& context = *atom::GetLuaContext(L);
    lua_pushinteger(L, static_cast<lua_Integer>(Sfx(context, L).GetLoadedCount()));
    return 1;
}

} // namespace

namespace atom {

auto RegisterSFXToLua(lua_State* L, LuaContext& context) -> void {
    const int top = lua_gettop(L);
    PushNamespace(L, {"Atom", "Audio"});
    lua_newtable(L);

    const luaL_Reg functions[] = {{"Load", LuaSfxLoad},
                                  {"Play", LuaSfxPlay},
                                  {"Stop", LuaSfxStop},
                                  {"StopAll", LuaSfxStopAll},
                                  {"SetVolume", LuaSfxSetVolume},
                                  {"IsLoaded", LuaSfxIsLoaded},
                                  {"Unload", LuaSfxUnload},
                                  {"Reset", LuaSfxReset},
                                  {"GetLoadedCount", LuaSfxGetLoadedCount},
                                  {nullptr, nullptr}};
    luaL_setfuncs(L, functions, 0);

    lua_setfield(L, -2, "SFX"); // Atom.Audio.SFX = functions
    lua_settop(L, top);         // restore stack balance across registrations
    LOG_INFO(atom::log::core::Lua, "Bound Atom.Audio.SFX");
}

} // namespace atom

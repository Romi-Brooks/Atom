/**
  * @file           : LuaVolume.cpp
  * @author         : Romi Brooks
  * @brief          : Lua bindings for VolumeManager singleton
  * @attention      :
  * @date           : 2026/6/9
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// Third Party Library
#include "lua.hpp"

// Engine Headers
#include <Media/Audio/Manager/VolumeManager.hpp>

static int lua_Volume_SetMasterVolume(lua_State* L) {
    float volume = static_cast<float>(luaL_checknumber(L, 1));
    atom::VolumeManager::GetInstance().SetMasterVolume(volume);
    return 0;
}

static int lua_Volume_GetMasterVolume(lua_State* L) {
    lua_pushnumber(L, atom::VolumeManager::GetInstance().GetMasterVolume());
    return 1;
}

static int lua_Volume_SetSfxVolume(lua_State* L) {
    float volume = static_cast<float>(luaL_checknumber(L, 1));
    atom::VolumeManager::GetInstance().SetSfxVolume(volume);
    return 0;
}

static int lua_Volume_GetSfxVolume(lua_State* L) {
    lua_pushnumber(L, atom::VolumeManager::GetInstance().GetSfxVolume());
    return 1;
}

static int lua_Volume_SetMusicVolume(lua_State* L) {
    float volume = static_cast<float>(luaL_checknumber(L, 1));
    atom::VolumeManager::GetInstance().SetMusicVolume(volume);
    return 0;
}

static int lua_Volume_GetMusicVolume(lua_State* L) {
    lua_pushnumber(L, atom::VolumeManager::GetInstance().GetMusicVolume());
    return 1;
}

auto RegisterVolumeToLua(lua_State* L) -> void {
    lua_newtable(L);

    const luaL_Reg volumeFunctions[] = {
        {"SetMasterVolume", lua_Volume_SetMasterVolume},
        {"GetMasterVolume", lua_Volume_GetMasterVolume},
        {"SetSfxVolume",    lua_Volume_SetSfxVolume},
        {"GetSfxVolume",    lua_Volume_GetSfxVolume},
        {"SetMusicVolume",  lua_Volume_SetMusicVolume},
        {"GetMusicVolume",  lua_Volume_GetMusicVolume},
        {nullptr, nullptr}
    };
    luaL_setfuncs(L, volumeFunctions, 0);

    lua_setglobal(L, "VolumeManager");
}

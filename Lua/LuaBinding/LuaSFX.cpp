/**
  * @file           : LuaSFX.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/10/12
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <string>

// Third Party Library
#include "lua.hpp"

// Engine Headers
#include <Media/Audio/Playback/SFXPlayer.hpp>

#include <Log/LogSystem.hpp>

// Global bridge pointer — set by the application after constructing SFX
// 全局桥接指针 — 由应用在构造 SFX 后设置
namespace {
atom::SFXPlayer* g_sfx = nullptr;
}

auto SetLuaSFXInstance(atom::SFXPlayer& sfx) -> void {
    g_sfx = &sfx;
}

static int lua_SFX_Load(lua_State* L) {
    const char* sfx_id = luaL_checkstring(L, 1);
    const char* file_path = luaL_checkstring(L, 2);

    bool load_result = g_sfx->Load(sfx_id, file_path);

    lua_pushboolean(L, load_result);
    return 1;
}

static int lua_SFX_Play(lua_State* L) {
    const char* sfx_id = luaL_checkstring(L, 1);
    g_sfx->Play(sfx_id);
    return 0;
}

static int lua_SFX_Stop(lua_State* L) {
    const char* sfx_id = luaL_checkstring(L, 1);
    g_sfx->Stop(sfx_id);
    return 0;
}

static int lua_SFX_StopAll(lua_State* L) {
    g_sfx->StopAll();
    return 0;
}

static int lua_SFX_SetVolume(lua_State* L) {
    const char* sfx_id = luaL_checkstring(L, 1);
    float volume = static_cast<float>(luaL_checknumber(L, 2));

    g_sfx->SetVolume(sfx_id, volume);
    return 0;
}

static int lua_SFX_IsLoaded(lua_State* L) {
    const char* sfx_id = luaL_checkstring(L, 1);
    bool is_loaded = g_sfx->IsLoaded(sfx_id);

    lua_pushboolean(L, is_loaded);
    return 1;
}

static int lua_SFX_Reset(lua_State* L) {
    g_sfx->Reset();
    return 0;
}

static int lua_SFXManager_LoadSFXFiles(lua_State* L) {
    const char* buf_id = luaL_checkstring(L, 1);
    const char* file_path = luaL_checkstring(L, 2);

    bool load_result = g_sfx->Load(buf_id, file_path);
    lua_pushboolean(L, load_result);
    return 1;
}

static int lua_SFXManager_UnloadSFX(lua_State* L) {
    const char* buf_id = luaL_checkstring(L, 1);
    bool unload_result = g_sfx->Unload(buf_id);

    lua_pushboolean(L, unload_result);
    return 1;
}

static int lua_SFXManager_UnloadAll(lua_State* L) {
    g_sfx->Reset();
    return 0;
}

static int lua_SFXManager_HasSFX(lua_State* L) {
    const char* buf_id = luaL_checkstring(L, 1);
    bool has_buf = g_sfx->IsLoaded(buf_id);

    lua_pushboolean(L, has_buf);
    return 1;
}

static int lua_SFXManager_GetLoadedCount(lua_State* L) {
    size_t count = g_sfx->GetLoadedCount();
    lua_pushinteger(L, static_cast<lua_Integer>(count));
    return 1;
}

auto RegisterSFXToLua(lua_State* L) -> void {
    lua_newtable(L);
    const luaL_Reg sfx_method_list[] = {{"Load", lua_SFX_Load},           {"Play", lua_SFX_Play},
                                        {"Stop", lua_SFX_Stop},           {"StopAll", lua_SFX_StopAll},
                                        {"SetVolume", lua_SFX_SetVolume}, {"IsLoaded", lua_SFX_IsLoaded},
                                        {"Reset", lua_SFX_Reset},         {nullptr, nullptr}};

    luaL_setfuncs(L, sfx_method_list, 0);

    lua_setglobal(L, "SFX");

    lua_newtable(L);
    const luaL_Reg sfx_manager_method_list[] = {
        {"LoadSFXFiles", lua_SFXManager_LoadSFXFiles},     {"UnloadSFX", lua_SFXManager_UnloadSFX},
        {"UnloadAll", lua_SFXManager_UnloadAll},           {"HasSFX", lua_SFXManager_HasSFX},
        {"GetLoadedCount", lua_SFXManager_GetLoadedCount}, {nullptr, nullptr}};
    luaL_setfuncs(L, sfx_manager_method_list, 0);
    lua_setglobal(L, "AudioClipCache");

    LOG_INFO(atom::core::LogChannel::LUA, "Atom Audio SFXPlayer and AudioClipCache registered successfully.");
}

/**
  * @file           : LuaMusic.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/10/12
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Third Party Library
#include "lua.hpp"

// Engine Headers
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Audio/Transitions/MusicCrossfade.hpp>
#include <Log/LogSystem.hpp>

// Global bridge pointer — set by the application after constructing Music
// 全局桥接指针 — 由应用在构造 Music 后设置
namespace {
    atom::MusicPlayer* g_music = nullptr;
    atom::audio::MusicCrossfade* g_transition = nullptr;
}

auto SetLuaMusicInstance(atom::MusicPlayer& music) -> void {
    g_music = &music;
}

auto SetLuaMusicCrossfadeInstance(atom::audio::MusicCrossfade& transition) -> void {
    g_transition = &transition;
}

// Lua binding: Load music
// Lua绑定：加载音乐
static int lua_Music_Load(lua_State* L) {
	const char* id = luaL_checkstring(L, 1);
	const char* file = luaL_checkstring(L, 2);

	bool result = g_music->Load(id, file);
	lua_pushboolean(L, result);
	return 1;
}

// Lua binding: Play music
// Lua绑定：播放音乐
static int lua_Music_Play(lua_State* L) {
	const char* id = luaL_checkstring(L, 1);
	float volume = luaL_optnumber(L, 2, 100.0f);

	g_music->Play(id, volume);
	return 0;
}

// Lua binding: Stop music
// Lua绑定：停止音乐
static int lua_Music_Stop(lua_State* L) {
	const char* id = luaL_checkstring(L, 1);
	g_music->Stop(id);
	return 0;
}

// Lua binding: Set music volume
// Lua绑定：设置音乐音量
static int lua_Music_SetVolume(lua_State* L) {
	const char* id = luaL_checkstring(L, 1);
	float volume = luaL_checknumber(L, 2);
	g_music->SetVolume(id, volume);
	return 0;
}

// Lua binding: Check if music is loaded
// Lua绑定：检查音乐是否已加载
static int lua_Music_IsLoaded(lua_State* L) {
	const char* id = luaL_checkstring(L, 1);
	bool isLoaded = g_music->IsLoaded(id);
	lua_pushboolean(L, isLoaded);
	return 1;
}

static int lua_Music_Crossfade(lua_State* L) {
    if (!g_transition) return luaL_error(L, "MusicCrossfade is not attached");
    const char* target = luaL_checkstring(L, 1);
    const float duration = static_cast<float>(luaL_optnumber(L, 2, 2.0));
    lua_pushboolean(L, g_transition->Switch(target, duration));
    return 1;
}
static int lua_Music_CancelTransition(lua_State* L) {
    if (!g_transition) return luaL_error(L, "MusicCrossfade is not attached");
    g_transition->Cancel();
    return 0;
}
static int lua_Music_GetTransitionProgress(lua_State* L) {
    if (!g_transition) return luaL_error(L, "MusicCrossfade is not attached");
    lua_pushnumber(L, g_transition->GetProgress());
    return 1;
}
// Register Music-related functions to Lua environment
// 注册Music相关函数到Lua环境
auto RegisterMusicToLua(lua_State* L) -> void {
	lua_newtable(L);

	const luaL_Reg musicFunctions[] = {
		{"Load", lua_Music_Load},
		{"Play", lua_Music_Play},
		{"Stop", lua_Music_Stop},
		{"SetVolume", lua_Music_SetVolume},
		{"IsLoaded", lua_Music_IsLoaded},
        {"Crossfade", lua_Music_Crossfade},
        {"CancelTransition", lua_Music_CancelTransition},
        {"GetTransitionProgress", lua_Music_GetTransitionProgress},
		{nullptr, nullptr}
	};
	luaL_setfuncs(L, musicFunctions, 0);

	lua_setglobal(L, "Music");
	LOG_INFO(atom::LogChannel::ATOM_LUA, "Engine.Audio.Music registered successfully.");
}

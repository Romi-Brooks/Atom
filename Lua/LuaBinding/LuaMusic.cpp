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
#include <Media/Audio/Music.hpp>
#include <Log/LogSystem.hpp>

// Get Music singleton (simplified macro)
// 获取Music单例（简化宏）
#define GET_MUSIC_INSTANCE() (atom::Music::GetInstance())

// Lua binding: Load music
// Lua绑定：加载音乐
static int lua_Music_Load(lua_State* L) {
	// Check parameters: 2 strings (id and file)
	// 检查参数：2个字符串（id和file）
	const char* id = luaL_checkstring(L, 1);
	const char* file = luaL_checkstring(L, 2);

	// Call singleton method
	// 调用单例方法
	bool result = GET_MUSIC_INSTANCE().Load(id, file);
	lua_pushboolean(L, result); // Return whether loading succeeded
	return 1;
}

// Lua binding: Play music
// Lua绑定：播放音乐
static int lua_Music_Play(lua_State* L) {
	// Check parameters: 1 string id, optional volume parameter (default 100.0)
	// 检查参数：1个字符串id，可选参数volume（默认100.0）
	const char* id = luaL_checkstring(L, 1);
	float volume = luaL_optnumber(L, 2, 100.0f); // Second parameter is optional, default 100

	GET_MUSIC_INSTANCE().Play(id, volume);
	return 0; // No return value
}

// Lua binding: Stop music
// Lua绑定：停止音乐
static int lua_Music_Stop(lua_State* L) {
	const char* id = luaL_checkstring(L, 1);
	GET_MUSIC_INSTANCE().Stop(id);
	return 0;
}

// Lua binding: Set music volume
// Lua绑定：设置音乐音量
static int lua_Music_SetVolume(lua_State* L) {
	const char* id = luaL_checkstring(L, 1);
	float volume = luaL_checknumber(L, 2); // Must provide volume parameter
	GET_MUSIC_INSTANCE().SetVolume(id, volume);
	return 0;
}

// Lua binding: Check if music is loaded
// Lua绑定：检查音乐是否已加载
static int lua_Music_IsLoaded(lua_State* L) {
	const char* id = luaL_checkstring(L, 1);
	bool isLoaded = GET_MUSIC_INSTANCE().IsLoaded(id);
	lua_pushboolean(L, isLoaded);
	return 1;
}

// Register Music-related functions to Lua environment
// 注册Music相关函数到Lua环境
auto RegisterMusicToLua(lua_State* L) -> void {
	// Create a table to store Music-related functions
	// 创建一个表存储Music相关函数
	lua_newtable(L);

	// Register methods into the table
	// 注册方法到表中
	const luaL_Reg musicFunctions[] = {
		{"Load", lua_Music_Load},
		{"Play", lua_Music_Play},
		{"Stop", lua_Music_Stop},
		{"SetVolume", lua_Music_SetVolume},
		{"IsLoaded", lua_Music_IsLoaded},
		{nullptr, nullptr}
	};
	luaL_setfuncs(L, musicFunctions, 0);

	// Set the table as global variable "Music". In Lua, call via Music:Load(...)
	// 将表设置为全局变量"Music"，Lua中可通过Music:Load(...)调用
	lua_setglobal(L, "Music");
	LOG_INFO(atom::LogChannel::ATOM_LUA, "Engine.Audio.Music registered successfully.");
}

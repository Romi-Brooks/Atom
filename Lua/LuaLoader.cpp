/**
  * @file           : LuaLoader.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/10/11
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <iostream>
#include <fstream>
#include <filesystem>

// Engine Headers
#include <Log/LogSystem.hpp>

// Self Dependency
#include "LuaLoader.hpp"

namespace fs = std::filesystem;

LuaLoader::LuaLoader() : L_(nullptr) {}

LuaLoader::~LuaLoader() {
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
    }
}

auto LuaLoader::Initialize() -> bool {
    // Create Lua state machine
    // 创建Lua状态机
    L_ = luaL_newstate();
    if (!L_) {
    	LOG_ERROR(atom::LogChannel::ATOM_LUA, "Failed to create Lua state!");
        return false;
    }

    // Open Lua standard libraries
    // 打开Lua标准库
	luaL_openlibs(L_); // Load Lua standard libraries
	RegisterEntityToLua(L_); // Register Entity bindings
	RegisterMusicToLua(L_);  // Register Music bindings
	RegisterSFXToLua(L_);     // Register SFXPlayer bindings
	RegisterVolumeToLua(L_);  // Register AudioMixer bindings

    return true;
}

auto LuaLoader::LoadScript(const std::string& scriptPath) -> bool {
    if (!L_ || !fs::exists(scriptPath)) {
    	LOG_ERROR(atom::LogChannel::ATOM_LUA, "Script file not found: " + scriptPath);
        return false;
    }

    // Record script path for hot-reload
    // 记录脚本路径，用于热重载
    loaded_scripts_[scriptPath] = scriptPath;

    // Load and execute the script
    // 加载并执行脚本
	if (const int result = luaL_dofile(L_, scriptPath.c_str()); result != LUA_OK) {
        HandleError(result);
        return false;
    }

	LOG_INFO(atom::LogChannel::ATOM_LUA, "Successfully loaded script: "  + scriptPath);
    return true;
}

auto LuaLoader::ReloadScript(const std::string& scriptPath) -> bool {
    if (!loaded_scripts_.contains(scriptPath)) {
    	LOG_ERROR(atom::LogChannel::ATOM_LUA, "Script not loaded: "  + scriptPath);
    	return false;
    }

    // Clear module cache
    // 清除模块缓存
    lua_getglobal(L_, "package");
    lua_getfield(L_, -1, "loaded");
    lua_pushnil(L_);
    lua_setfield(L_, -2, scriptPath.c_str());
    lua_pop(L_, 2);

    // Reload
    // 重新加载
    return LoadScript(scriptPath);
}

auto LuaLoader::CallLuaFunction(const std::string& funcName) const -> bool {
    if (!L_) return false;

    // Find the function and push it onto the stack
    // 查找函数并压入栈
    lua_getglobal(L_, funcName.c_str());

    // Check if it is a function
    // 检查是否是函数
    if (!lua_isfunction(L_, -1)) {
    	LOG_ERROR(atom::LogChannel::ATOM_LUA, "Lua function not found: "  + funcName);
        lua_pop(L_, 1);
        return false;
    }

    // Call the function (0 arguments, 0 return values)
    // 调用函数（0个参数，0个返回值）
	if (const int result = lua_pcall(L_, 0, 0, 0); result != LUA_OK) {
        HandleError(result);
        return false;
    }

    return true;
}

auto LuaLoader::RegisterEntity(atom::Entity* entity, const std::string& luaVarName) const -> void {
	if (L_ && entity) {
		PushEntityToLua(L_, entity, luaVarName);
		LOG_INFO(atom::LogChannel::ATOM_LUA, "Registered Entity to Lua as: "  + luaVarName);
	}
}

auto LuaLoader::HandleError(int result) const -> void {
	const char* errorMsg = lua_tostring(L_, -1);
	LOG_ERROR(atom::LogChannel::ATOM_LUA,  "Lua error: " + std::to_string(*errorMsg));
    lua_pop(L_, 1); // Clean up the stack
}

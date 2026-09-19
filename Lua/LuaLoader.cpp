/**
  * @file           : LuaLoader.cpp
  * @author         : Romi Brooks
  * @brief          : Lua state wrapper that loads scripts through the VFS.
  * @attention      :
  * @date           : 2025/10/11
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Self Dependency
#include "LuaLoader.hpp"

// Standard Library
#include <vector>

// Engine Headers
#include <Log/LogSystem.hpp>

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
        LOG_ERROR(atom::log::core::Lua, "Failed to create Lua state!");
        return false;
    }

    // Open Lua standard libraries
    // 打开Lua标准库
    luaL_openlibs(L_);       // Load Lua standard libraries
    RegisterMusicToLua(L_);  // Register Music bindings
    RegisterSFXToLua(L_);    // Register SFXPlayer bindings
    RegisterVolumeToLua(L_); // Register AudioMixer bindings

    return true;
}

auto LuaLoader::LoadScript(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) -> bool {
    if (!L_ || !path.IsValid()) {
        LOG_ERROR(atom::log::core::Lua, "LoadScript called without a Lua state or with an invalid path");
        return false;
    }

    std::unique_ptr<atom::fs::IFile> file{};
    if (filesystem.OpenRead(path, file) != atom::fs::Result::Success || !file) {
        LOG_ERROR(atom::log::core::Lua, "Script not found: " + std::string{path.String()});
        return false;
    }

    std::vector<std::byte> bytes{};
    if (atom::fs::ReadAll(*file, bytes) != atom::fs::Result::Success) {
        LOG_ERROR(atom::log::core::Lua, "Failed to read script: " + std::string{path.String()});
        return false;
    }

    const std::string_view source{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    return LoadScriptSource(path.String(), source);
}

auto LuaLoader::LoadScriptSource(const std::string_view chunk_name, const std::string_view source) -> bool {
    if (!L_)
        return false;

    const std::string chunk{chunk_name};
    // Record script identity for hot-reload
    // 记录脚本身份，用于热重载
    loaded_scripts_[chunk] = chunk;

    if (luaL_loadbuffer(L_, source.data(), source.size(), chunk.c_str()) != LUA_OK) {
        HandleError(LUA_ERRSYNTAX);
        return false;
    }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        HandleError(LUA_ERRRUN);
        return false;
    }

    LOG_INFO(atom::log::core::Lua, "Successfully loaded script: " + chunk);
    return true;
}

auto LuaLoader::ReloadScript(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) -> bool {
    if (!L_ || !path.IsValid())
        return false;

    const std::string chunk{path.String()};
    if (!loaded_scripts_.contains(chunk)) {
        LOG_ERROR(atom::log::core::Lua, "Script not loaded: " + chunk);
        return false;
    }

    // Clear module cache
    // 清除模块缓存
    lua_getglobal(L_, "package");
    lua_getfield(L_, -1, "loaded");
    lua_pushnil(L_);
    lua_setfield(L_, -2, chunk.c_str());
    lua_pop(L_, 2);

    return LoadScript(filesystem, path);
}

auto LuaLoader::CallLuaFunction(const std::string& funcName) const -> bool {
    if (!L_)
        return false;

    // Find the function and push it onto the stack
    // 查找函数并压入栈
    lua_getglobal(L_, funcName.c_str());

    // Check if it is a function
    // 检查是否是函数
    if (!lua_isfunction(L_, -1)) {
        LOG_ERROR(atom::log::core::Lua, "Lua function not found: " + funcName);
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

auto LuaLoader::HandleError(int /*result*/) const -> void {
    const char* errorMsg = lua_tostring(L_, -1);
    const std::string message = errorMsg ? std::string{errorMsg} : std::string{"unknown Lua error"};
    LOG_ERROR(atom::log::core::Lua, "Lua error: " + message);
    lua_pop(L_, 1); // Clean up the stack
}

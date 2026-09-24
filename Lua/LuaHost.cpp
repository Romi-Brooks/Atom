/**
 * @file           : LuaHost.cpp
 * @brief          : Lua state lifecycle, binding application and script IO.
**/

#include "LuaHost.hpp"

#include <memory>
#include <vector>

#include <Log/LogSystem.hpp>
#include <Lua/LuaBinding/ContextBridge.hpp>

namespace atom {

LuaHost::LuaHost() {
    // Default bindings. Extra modules can be appended with AddBinding() before
    // Initialize(); they run in registration order.
    AddBinding(RegisterMusicToLua);
    AddBinding(RegisterSFXToLua);
    AddBinding(RegisterVolumeToLua);
    AddBinding(RegisterLogToLua);
}

LuaHost::~LuaHost() {
    if (state_) {
        lua_close(state_);
        state_ = nullptr;
    }
}

auto LuaHost::Initialize() -> bool {
    state_ = luaL_newstate();
    if (!state_) {
        LOG_ERROR(atom::log::core::Lua, "Failed to create Lua state");
        return false;
    }

    luaL_openlibs(state_);
    SetLuaContext(state_, context_);
    registry_.RegisterAll(state_, context_);

    LOG_INFO(atom::log::core::Lua,
             "Lua state initialized with " + std::to_string(registry_.Count()) + " binding module(s)");
    return true;
}

auto LuaHost::LoadScript(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) -> bool {
    if (!state_ || !path.IsValid()) {
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

auto LuaHost::LoadScriptSource(const std::string_view chunk_name, const std::string_view source) -> bool {
    if (!state_)
        return false;

    const std::string chunk{chunk_name};
    loaded_scripts_[chunk] = chunk;

    if (luaL_loadbuffer(state_, source.data(), source.size(), chunk.c_str()) != LUA_OK) {
        HandleError(LUA_ERRSYNTAX);
        return false;
    }
    if (lua_pcall(state_, 0, 0, 0) != LUA_OK) {
        HandleError(LUA_ERRRUN);
        return false;
    }

    LOG_INFO(atom::log::core::Lua, "Successfully loaded script: " + chunk);
    return true;
}

auto LuaHost::ReloadScript(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) -> bool {
    if (!state_ || !path.IsValid())
        return false;

    const std::string chunk{path.String()};
    if (!loaded_scripts_.contains(chunk)) {
        LOG_ERROR(atom::log::core::Lua, "Script not loaded: " + chunk);
        return false;
    }

    lua_getglobal(state_, "package");
    lua_getfield(state_, -1, "loaded");
    lua_pushnil(state_);
    lua_setfield(state_, -2, chunk.c_str());
    lua_pop(state_, 2);

    return LoadScript(filesystem, path);
}

auto LuaHost::CallLuaFunction(const std::string& func_name) const -> bool {
    if (!state_)
        return false;

    lua_getglobal(state_, func_name.c_str());
    if (!lua_isfunction(state_, -1)) {
        LOG_ERROR(atom::log::core::Lua, "Lua function not found: " + func_name);
        lua_pop(state_, 1);
        return false;
    }

    if (const int result = lua_pcall(state_, 0, 0, 0); result != LUA_OK) {
        HandleError(result);
        return false;
    }

    return true;
}

auto LuaHost::HandleError(const int /*result*/) const -> void {
    const char* error_msg = lua_tostring(state_, -1);
    const std::string message = error_msg ? std::string{error_msg} : std::string{"unknown Lua error"};
    LOG_ERROR(atom::log::core::Lua, "Lua error: " + message);
    lua_pop(state_, 1);
}

} // namespace atom

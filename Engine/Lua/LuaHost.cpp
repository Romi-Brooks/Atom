/**
 * @file           : LuaHost.cpp
 * @brief          : Lua state lifecycle, binding application and script IO.
**/

#include "LuaHost.hpp"

#include <memory>
#include <string>
#include <vector>

#include <Log/LogSystem.hpp>
#include <Lua/LuaBinding/ContextBridge.hpp>
#include <Lua/LuaGuard.hpp>

namespace atom {

namespace {

// Human-readable name for a lua_pcall/luaL_loadbuffer status code, so a failure
// log says which stage failed (ARCH-005).
auto LuaStatusName(int status) -> const char* {
    switch (status) {
    case LUA_OK:
        return "ok";
    case LUA_YIELD:
        return "yield";
    case LUA_ERRRUN:
        return "runtime error";
    case LUA_ERRSYNTAX:
        return "syntax error";
    case LUA_ERRMEM:
        return "memory error";
    case LUA_ERRERR:
        return "error handler error";
    default:
        return "unknown error";
    }
}

} // namespace

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
    if (state_) {
        LOG_WARNING(atom::log::core::Lua, "Initialize() ignored: the Lua state is already created");
        return true;
    }

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
    if (!state_) {
        LOG_ERROR(atom::log::core::Lua, "LoadScriptSource called without a Lua state");
        return false;
    }

    // The guard restores the stack on every exit path, including the error paths
    // below (ARCH-005).
    const LuaStackGuard guard{state_};
    const std::string chunk{chunk_name};

    if (luaL_loadbuffer(state_, source.data(), source.size(), chunk.c_str()) != LUA_OK) {
        HandleError(LUA_ERRSYNTAX);
        return false;
    }

    if (const int result = lua_pcall(state_, 0, 0, 0); result != LUA_OK) {
        HandleError(result);
        return false;
    }

    // Only a script that actually ran is tracked for ReloadScript.
    loaded_scripts_.insert_or_assign(chunk, chunk);
    LOG_INFO(atom::log::core::Lua, "Successfully loaded script: " + chunk);
    return true;
}

auto LuaHost::ReloadScript(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) -> bool {
    if (!state_ || !path.IsValid()) {
        LOG_ERROR(atom::log::core::Lua, "ReloadScript called without a Lua state or with an invalid path");
        return false;
    }

    const std::string chunk{path.String()};
    if (!loaded_scripts_.contains(chunk)) {
        LOG_ERROR(atom::log::core::Lua, "Script not loaded: " + chunk);
        return false;
    }

    {
        // Drop the cached module so the reload actually re-executes the chunk.
        // `package` is absent in a sandboxed state, which used to index a nil
        // value and raise an uncontrolled error from C (ARCH-005).
        const LuaStackGuard guard{state_};
        lua_getglobal(state_, "package");
        if (lua_istable(state_, -1)) {
            lua_getfield(state_, -1, "loaded");
            if (lua_istable(state_, -1)) {
                lua_pushnil(state_);
                lua_setfield(state_, -2, chunk.c_str());
            }
        }
    }

    return LoadScript(filesystem, path);
}

auto LuaHost::CallLuaFunction(const std::string& func_name) const -> bool {
    if (!state_) {
        LOG_ERROR(atom::log::core::Lua, "CallLuaFunction called without a Lua state");
        return false;
    }

    const LuaStackGuard guard{state_};

    lua_getglobal(state_, func_name.c_str());
    if (!lua_isfunction(state_, -1)) {
        LOG_ERROR(atom::log::core::Lua, "Lua function not found: " + func_name);
        return false;
    }

    if (const int result = lua_pcall(state_, 0, 0, 0); result != LUA_OK) {
        HandleError(result);
        return false;
    }

    return true;
}

auto LuaHost::HandleError(const int result) const -> void {
    // The error object sits on top of the stack. It is not required to be a
    // string: pcall can raise tables, userdata or nil (ARCH-005), and
    // lua_tostring() returns nullptr for those — which used to lose the message
    // entirely.
    std::string message = "unknown Lua error";

    if (state_ && lua_gettop(state_) > 0) {
        if (const char* text = lua_tostring(state_, -1)) {
            message = text;
        } else {
            // Render any value type under protection: a hostile __tostring must
            // not be able to longjmp out of this unprotected C frame.
            AppendLuaValue(state_, lua_gettop(state_), message);
        }
        lua_pop(state_, 1); // the original error object
    }

    LOG_ERROR(atom::log::core::Lua,
              std::string{"Lua "} + LuaStatusName(result) + " (" + std::to_string(result) + "): " + message);
}

} // namespace atom

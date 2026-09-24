/**
 * @file           : LuaLog.cpp
 * @brief          : Atom.Log bindings routing Lua output into the engine Log.
 * @attention      : Lua's stock `print` writes straight to stdout and bypasses
 *                   LogSystem (and thus the LogDebugger / console). Mod scripts
 *                   that want engine-managed logging call Atom.Log.* instead;
 *                   `print` is intentionally left untouched so a script's own
 *                   stdout usage is never captured against its will.
**/

#include "ContextBridge.hpp"
#include "lua.hpp"

#include <string>

#include <Log/LogSystem.hpp>

namespace {

// Concatenate every argument on the stack (like Lua's print) into one message.
// Argument 1 is the first value; the binding function passes the argument count
// so this helper can consume the whole call frame.
auto ConcatArguments(lua_State* L, int count) -> std::string {
    std::string message;
    for (int i = 1; i <= count; ++i) {
        if (i > 1)
            message += '\t';
        size_t len = 0;
        const char* text = luaL_tolstring(L, i, &len);
        message.append(text, len);
        lua_pop(L, 1); // drop the tolstring copy
    }
    return message;
}

template <atom::LogLevel Level>
int LuaLogEmit(lua_State* L) {
    const int count = lua_gettop(L);
    const std::string message = ConcatArguments(L, count);
    atom::Log::LogOut(atom::ResolveChannelPrefix(atom::log::core::Lua),
                      atom::ResolveChannelName(atom::log::core::Lua), Level, message);
    return 0;
}

int LuaLogInfo(lua_State* L) {
    return LuaLogEmit<atom::LogLevel::ATOM_INFO>(L);
}

int LuaLogWarning(lua_State* L) {
    return LuaLogEmit<atom::LogLevel::ATOM_WARNING>(L);
}

int LuaLogError(lua_State* L) {
    return LuaLogEmit<atom::LogLevel::ATOM_ERROR>(L);
}

int LuaLogDebug(lua_State* L) {
    return LuaLogEmit<atom::LogLevel::ATOM_DEBUG>(L);
}

} // namespace

namespace atom {

auto RegisterLogToLua(lua_State* L, LuaContext& context) -> void {
    const int top = lua_gettop(L);
    PushNamespace(L, {"Atom"});
    lua_newtable(L);

    const luaL_Reg functions[] = {{"Info", LuaLogInfo},
                                  {"Warning", LuaLogWarning},
                                  {"Error", LuaLogError},
                                  {"Debug", LuaLogDebug},
                                  {nullptr, nullptr}};
    luaL_setfuncs(L, functions, 0);

    lua_setfield(L, -2, "Log"); // Atom.Log = functions
    lua_settop(L, top);         // restore stack balance across registrations
    LOG_INFO(atom::log::core::Lua, "Bound Atom.Log");
}

} // namespace atom

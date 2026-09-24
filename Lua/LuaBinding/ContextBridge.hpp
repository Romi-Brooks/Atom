/**
 * @file           : ContextBridge.hpp
 * @brief          : Stores the active LuaContext in the Lua registry so any
 *                   binding can retrieve it without per-closure upvalues.
 * @attention      : The context pointer is owned by LuaHost and outlives the
 *                   state; it is stored once at Initialize and read by bindings.
**/

#ifndef ATOM_LUA_CONTEXT_BRIDGE_HPP
#define ATOM_LUA_CONTEXT_BRIDGE_HPP

#include "lua.hpp"

#include <initializer_list>

#include <Lua/LuaContext.hpp>

namespace atom {

// Stash the context pointer under a private lightuserdata key in the registry.
inline auto SetLuaContext(lua_State* L, LuaContext& context) -> void {
    lua_pushlightuserdata(L, &context);
    lua_setfield(L, LUA_REGISTRYINDEX, "__atom_lua_context");
}

// Retrieve the context; returns nullptr when SetLuaContext was not called.
inline auto GetLuaContext(lua_State* L) -> LuaContext* {
    lua_getfield(L, LUA_REGISTRYINDEX, "__atom_lua_context");
    auto* context = static_cast<LuaContext*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return context;
}

// Build (or reuse) a nested table path such as { "Atom", "Audio" } and leave
// the innermost table on top of the stack. Each intermediate table is stored on
// its parent (the first under the global table), so multiple bindings compose
// into one shared namespace tree.
inline auto PushNamespace(lua_State* L, std::initializer_list<const char*> path) -> void {
    for (const char* segment : path) {
        const bool is_root = (lua_gettop(L) == 0);
        // Fetch the child table: from the global table for the first segment,
        // otherwise from the parent table left on top of the stack.
        if (is_root)
            lua_getglobal(L, segment);
        else
            lua_getfield(L, -1, segment);

        if (lua_istable(L, -1))
            continue; // already exists; it is now the parent for the next segment

        // Missing: create it and attach it to its parent.
        lua_pop(L, 1);       // drop the nil
        lua_newtable(L);     // new child, now on top
        if (is_root) {
            lua_pushvalue(L, -1);
            lua_setglobal(L, segment);
        } else {
            lua_pushvalue(L, -1);      // duplicate child
            lua_setfield(L, -3, segment); // parent[segment] = child
        }
    }
}

} // namespace atom

#endif // ATOM_LUA_CONTEXT_BRIDGE_HPP

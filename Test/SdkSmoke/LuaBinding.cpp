#include <Atom/Lua/LuaHost.hpp>

namespace {

auto Twice(lua_State* state) -> int {
    lua_pushinteger(state, luaL_checkinteger(state, 1) * 2);
    return 1;
}

auto RegisterSdkBinding(lua_State* state, atom::LuaContext&) -> void {
    lua_pushcfunction(state, &Twice);
    lua_setglobal(state, "sdk_twice");
}

} // namespace

auto main() -> int {
    atom::LuaHost host;
    host.AddBinding(&RegisterSdkBinding);
    if (!host.Initialize()) {
        return 1;
    }
    if (!host.LoadScriptSource("sdk-binding", "sdk_result = sdk_twice(21)")) {
        return 2;
    }

    auto* state = host.GetLuaState();
    if (lua_getglobal(state, "sdk_result") != LUA_TNUMBER || lua_tointeger(state, -1) != 42) {
        return 3;
    }
    lua_pop(state, 1);
    return lua_gettop(state) == 0 ? 0 : 4;
}

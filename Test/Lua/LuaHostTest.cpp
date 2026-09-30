/**
 * @file           : LuaHostTest.cpp
 * @brief          : ARCH-005 — Lua error handling, stack balance and null
 *                   subsystem injection.
 */

#include <Filesystem/MemoryFileSystem.hpp>
#include <Log/LogSystem.hpp>
#include <Lua/LuaBinding/ContextBridge.hpp>
#include <Lua/LuaGuard.hpp>
#include <Lua/LuaHost.hpp>
#include <Lua/lua.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

} // namespace

auto main() -> int {
    // Keep the console quiet; listeners still receive every record.
    atom::Log::SetConsoleSinkEnabled(false);

    std::vector<atom::LogRecord> records{};
    auto connection = atom::Log::Subscribe([&records](const atom::LogRecord& record) { records.push_back(record); });

    // Most recent message emitted on the engine's Lua channel.
    const auto last_lua_error = [&records]() -> std::string {
        for (auto it = records.rbegin(); it != records.rend(); ++it) {
            if (it->channel_name == "Lua" && it->level == atom::LogLevel::ATOM_ERROR)
                return it->message;
        }
        return {};
    };

    atom::LuaHost host{};
    ATOM_CHECK(host.Initialize());
    // Re-initializing must not leak a second state.
    ATOM_CHECK(host.Initialize());

    auto* state = host.GetLuaState();
    ATOM_CHECK(state != nullptr);
    const int base_top = lua_gettop(state);

    // --- syntax error -------------------------------------------------------
    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("bad_syntax", "this is not lua ("));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("syntax error") != std::string::npos);

    // --- runtime error ------------------------------------------------------
    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("bad_runtime", "error('boom-runtime')"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("boom-runtime") != std::string::npos);

    // --- non-string error objects ------------------------------------------
    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("bad_table", "error({ code = 7, reason = 'table-error' })"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("table") != std::string::npos);

    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("bad_nil", "error()"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("nil") != std::string::npos);

    // A raising __tostring must not escape the protected rendering path, and the
    // resulting message must still say what the value was and why it failed.
    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("bad_tostring",
                                      "local t = setmetatable({}, { __tostring = function() "
                                      "error('tostring-exploded') end })\n"
                                      "error(t)"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("table") != std::string::npos);
    ATOM_CHECK(last_lua_error().find("tostring-exploded") != std::string::npos);

    // --- null subsystem injection ------------------------------------------
    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("null_music", "Atom.Audio.Music.Play('song')"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("not attached") != std::string::npos);

    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("null_sfx", "Atom.Audio.SFX.StopAll()"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("not attached") != std::string::npos);

    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("null_mixer", "Atom.Audio.Mixer.SetMasterVolume(50)"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("not attached") != std::string::npos);

    // --- bad arguments are controlled errors -------------------------------
    records.clear();
    ATOM_CHECK(!host.LoadScriptSource("bad_arg", "Atom.Audio.SFX.Load()"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(!last_lua_error().empty());

    // --- success paths keep the stack balanced -----------------------------
    records.clear();
    ATOM_CHECK(host.LoadScriptSource("good", "function ping() return 1 end"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(host.CallLuaFunction("ping"));
    ATOM_CHECK(lua_gettop(state) == base_top);

    ATOM_CHECK(!host.CallLuaFunction("not_a_function"));
    ATOM_CHECK(lua_gettop(state) == base_top);

    records.clear();
    ATOM_CHECK(host.LoadScriptSource("raiser", "function raiser() error('raised-from-function') end"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(!host.CallLuaFunction("raiser"));
    ATOM_CHECK(lua_gettop(state) == base_top);
    ATOM_CHECK(last_lua_error().find("raised-from-function") != std::string::npos);

    // --- VFS loading and reload --------------------------------------------
    std::unique_ptr<atom::fs::MemoryFileSystem> owned{};
    ATOM_CHECK(atom::fs::MemoryFileSystem::Create("res", owned) == atom::fs::Result::Success && owned);

    atom::fs::AssetPath script_path{};
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://scripts/reload.lua", script_path));

    // Reloading something that was never loaded is a clean failure.
    ATOM_CHECK(!host.ReloadScript(*owned, script_path));
    ATOM_CHECK(lua_gettop(state) == base_top);

    ATOM_CHECK(owned->WriteFile(script_path, Bytes("counter = (counter or 0) + 1")) == atom::fs::Result::Success);
    ATOM_CHECK(host.LoadScript(*owned, script_path));
    ATOM_CHECK(lua_gettop(state) == base_top);

    lua_getglobal(state, "counter");
    ATOM_CHECK(lua_tointeger(state, -1) == 1);
    lua_pop(state, 1);

    ATOM_CHECK(host.ReloadScript(*owned, script_path));
    ATOM_CHECK(lua_gettop(state) == base_top);
    lua_getglobal(state, "counter");
    // ReloadScript promises re-execution of the chunk. (The package.loaded
    // clearing matters for `require`, not for this explicit load.)
    ATOM_CHECK(lua_tointeger(state, -1) == 2);
    lua_pop(state, 1);

    // A state without the `package` library used to make the reload index a nil
    // value and raise an uncontrolled error from C. It must now succeed.
    lua_pushnil(state);
    lua_setglobal(state, "package");
    ATOM_CHECK(host.ReloadScript(*owned, script_path));
    ATOM_CHECK(lua_gettop(state) == base_top);
    lua_getglobal(state, "counter");
    ATOM_CHECK(lua_tointeger(state, -1) == 3);
    lua_pop(state, 1);

    // --- a state without the injected context ------------------------------
    {
        lua_State* bare = luaL_newstate();
        ATOM_CHECK(bare != nullptr);
        luaL_openlibs(bare);
        atom::LuaContext context{};
        // The binding is registered, but the context was never stashed.
        atom::RegisterVolumeToLua(bare, context);
        const int bare_top = lua_gettop(bare);
        ATOM_CHECK(luaL_dostring(bare, "Atom.Audio.Mixer.SetMasterVolume(50)") != LUA_OK);
        const char* message = lua_tostring(bare, -1);
        ATOM_CHECK(message != nullptr);
        ATOM_CHECK(std::string{message}.find("context") != std::string::npos);
        lua_pop(bare, 1);
        ATOM_CHECK(lua_gettop(bare) == bare_top);

        // With the context installed but no mixer injected, the binding reports a
        // controlled error instead of dereferencing a null subsystem.
        atom::SetLuaContext(bare, context);
        ATOM_CHECK(luaL_dostring(bare, "Atom.Audio.Mixer.SetMasterVolume(50)") != LUA_OK);
        message = lua_tostring(bare, -1);
        ATOM_CHECK(message != nullptr);
        ATOM_CHECK(std::string{message}.find("not attached") != std::string::npos);
        lua_pop(bare, 1);
        ATOM_CHECK(lua_gettop(bare) == bare_top);
        lua_close(bare);
    }

    // --- LuaStackGuard ------------------------------------------------------
    {
        const int guard_top = lua_gettop(state);
        {
            const atom::LuaStackGuard guard{state};
            lua_pushinteger(state, 1);
            lua_pushinteger(state, 2);
            ATOM_CHECK(guard.LeakedSlots() == 2);
        }
        ATOM_CHECK(lua_gettop(state) == guard_top);
    }

    connection.Reset();
    return 0;
}

/**
 * @file           : LuaHost.hpp
 * @brief          : Single owner of a Lua state, its bindings and script IO.
 * @attention      : Replaces LuaLoader. LuaHost is the only place that holds a
 *                   lua_State*; it injects a LuaContext into every binding and
 *                   loads scripts through the VFS.
**/

#ifndef ATOM_LUA_HOST_HPP
#define ATOM_LUA_HOST_HPP

#include <string>
#include <string_view>
#include <unordered_map>

#include "BindingRegistry.hpp"
#include "LuaContext.hpp"
#include "lua.hpp"

#include <Filesystem/AssetPath.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom {

// Binding registrars (defined in LuaBinding/*.cpp). Each creates one table under
// the Atom.Audio namespace.
auto RegisterMusicToLua(lua_State* L, LuaContext& context) -> void;
auto RegisterSFXToLua(lua_State* L, LuaContext& context) -> void;
auto RegisterVolumeToLua(lua_State* L, LuaContext& context) -> void;
auto RegisterLogToLua(lua_State* L, LuaContext& context) -> void;

class LuaHost final {
    public:
        LuaHost();
        ~LuaHost();

        LuaHost(const LuaHost&) = delete;
        LuaHost& operator=(const LuaHost&) = delete;

        // Register a binding module before Initialize(). Bindings are applied in
        // registration order when the state is created.
        auto AddBinding(BindingRegistrar registrar) -> void {
            registry_.Add(registrar);
        }

        // Create the Lua state, open the standard libraries and apply every
        // registered binding against `context_`.
        auto Initialize() -> bool;

        // Load and execute a script from the VFS.
        auto LoadScript(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) -> bool;

        // Execute in-memory source (e.g. a cached Script resource).
        auto LoadScriptSource(std::string_view chunk_name, std::string_view source) -> bool;

        // Reload a previously loaded script (hot reload).
        auto ReloadScript(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) -> bool;

        // Call a global Lua function with no arguments or return values.
        auto CallLuaFunction(const std::string& func_name) const -> bool;

        // The context bindings read from. Fill it before Initialize().
        [[nodiscard]] auto Context() -> LuaContext& {
            return context_;
        }

        [[nodiscard]] auto Context() const -> const LuaContext& {
            return context_;
        }

        [[nodiscard]] auto GetLuaState() const -> lua_State* {
            return state_;
        }

    private:
        auto HandleError(int result) const -> void;

        lua_State* state_ = nullptr;
        LuaContext context_{};
        BindingRegistry registry_{};
        std::unordered_map<std::string, std::string> loaded_scripts_{};
};

} // namespace atom

#endif // ATOM_LUA_HOST_HPP

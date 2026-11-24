/**
 * @file           : BindingRegistry.hpp
 * @brief          : Ordered registry of Lua binding registrars.
 * @attention      : A binding module exports one `RegisterXxx(lua_State*,
 *                   LuaContext&)` function and appends it here; LuaHost then
 *                   walks the list at Initialize. Adding a binding touches this
 *                   single registration point instead of three places.
**/

#ifndef ATOM_LUA_BINDING_REGISTRY_HPP
#define ATOM_LUA_BINDING_REGISTRY_HPP

#include <vector>

#include "LuaContext.hpp"

struct lua_State;

namespace atom {

// Signature every binding registrar must match. `L` is the freshly created
// state; `context` supplies the engine objects to bind.
using BindingRegistrar = void (*)(lua_State* L, LuaContext& context);

class BindingRegistry final {
    public:
        auto Add(BindingRegistrar registrar) -> void {
            registrars_.push_back(registrar);
        }

        auto RegisterAll(lua_State* L, LuaContext& context) const -> void {
            for (const auto registrar : registrars_)
                registrar(L, context);
        }

        [[nodiscard]] auto Count() const -> std::size_t {
            return registrars_.size();
        }

    private:
        std::vector<BindingRegistrar> registrars_{};
};

} // namespace atom

#endif // ATOM_LUA_BINDING_REGISTRY_HPP

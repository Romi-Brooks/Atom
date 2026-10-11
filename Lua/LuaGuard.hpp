/**
 * @file           : LuaGuard.hpp
 * @brief          : RAII helpers that keep the shared Lua stack balanced.
 * @attention      : ARCH-005 - every Lua entry point in LuaHost and every
 *                   binding runs under a guard so a syntax error, a runtime
 *                   error, a non-string error object or an early return cannot
 *                   leak stack slots into the next call.
**/

#ifndef ATOM_LUA_GUARD_HPP
#define ATOM_LUA_GUARD_HPP

#include <cstddef>
#include <string>

#include <Lua/lua.hpp>

namespace atom {

// Records lua_gettop() on construction and restores it on scope exit. Cheaper
// and less error-prone than matching every lua_push*/lua_pop* pair by hand.
class LuaStackGuard final {
    public:
        explicit LuaStackGuard(lua_State* state) : state_(state), top_(state ? lua_gettop(state) : 0) {}

        ~LuaStackGuard() {
            if (state_)
                lua_settop(state_, top_);
        }

        LuaStackGuard(const LuaStackGuard&) = delete;
        auto operator=(const LuaStackGuard&) -> LuaStackGuard& = delete;
        LuaStackGuard(LuaStackGuard&&) = delete;
        auto operator=(LuaStackGuard&&) -> LuaStackGuard& = delete;

        [[nodiscard]] auto Top() const -> int {
            return top_;
        }

        // Number of slots this guard will discard on scope exit.
        [[nodiscard]] auto LeakedSlots() const -> int {
            return state_ ? lua_gettop(state_) - top_ : 0;
        }

    private:
        lua_State* state_ = nullptr;
        int top_ = 0;
};

namespace lua_detail {

// Runs as a protected Lua call: converts the value at stack index 1 to a string.
// A raising __tostring metamethod becomes an ordinary error here instead of a
// longjmp through C++ frames.
inline auto RenderValue(lua_State* state) -> int {
    size_t length = 0;
    // luaL_tolstring() raises rather than returning NULL for a hostile
    // metamethod; the NULL branch only guards a future Lua change.
    const char* text = luaL_tolstring(state, 1, &length);
    if (text == nullptr) {
        lua_pushliteral(state, "<unprintable>");
        return 1;
    }
    lua_pushlstring(state, text, length);
    return 1;
}

} // namespace lua_detail

// Appends a printable rendering of the value at `index` (an absolute, positive
// stack index) to `out` and leaves the stack unchanged.
//
// The conversion is protected on purpose: luaL_tolstring() honours __tostring,
// and a hostile metamethod that raises would otherwise longjmp past the caller's
// live std::string (skipping its destructor) or out of an unprotected C frame.
//
// Returns false when the value could not be rendered; a description is still
// appended in that case (the value's type plus the metamethod's own error), so
// the resulting message is never a bare placeholder. The return value is
// informational and may be ignored.
inline auto AppendLuaValue(lua_State* state, int index, std::string& out) -> bool {
    if (state == nullptr || index <= 0) {
        out += "<unprintable>";
        return false;
    }

    const LuaStackGuard guard{state};
    lua_pushcfunction(state, lua_detail::RenderValue);
    lua_pushvalue(state, index);
    if (lua_pcall(state, 1, 1, 0) != LUA_OK) {
        // Keep both halves of the story: what the value was and why rendering it
        // failed. `index` still refers to the original value (pushvalue copied
        // it), so the stack layout is unchanged for the caller.
        out += '<';
        out += luaL_typename(state, index);
        if (const char* render_error = lua_tostring(state, -1)) {
            out += " whose __tostring failed: ";
            out += render_error;
        } else {
            out += " whose __tostring failed>";
        }
        out += '>';
        lua_pop(state, 1); // the rendering error object
        return false;
    }

    size_t length = 0;
    if (const char* text = lua_tolstring(state, -1, &length)) {
        out.append(text, length);
        return true;
    }
    out += "<unprintable>";
    return false;
}

} // namespace atom

#endif // ATOM_LUA_GUARD_HPP
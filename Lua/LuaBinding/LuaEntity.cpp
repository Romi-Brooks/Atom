/**
  * @file           : Entity.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/10/12
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <string>

// Third Party Library
#include "lua.hpp"

// Engine Headers
#include <Components/Entities/Entity.hpp>
#include <Log/LogSystem.hpp>

using Entity = atom::Entity;

// Check if the userdata on the Lua stack is of Entity type
// 检查Lua栈中的userdata是否为Entity类型
#define CHECK_ENTITY(L) \
    Entity* entity = *static_cast<Entity**>(luaL_checkudata(L, 1, "EntityMetaTable")); \
    if (!entity) { \
        return luaL_error(L, "invalid Entity object"); \
    }

// Lua binding: Get HP
// Lua绑定：获取血量
static int lua_Entity_GetHP(lua_State* L) {
    CHECK_ENTITY(L);
    lua_pushinteger(L, entity->GetHP());
    return 1; // Return 1 value (HP)
}

// Lua binding: Set HP (hotfix may require dynamic HP adjustment)
// Lua绑定：设置血量（热修复可能需要动态调整血量）
static int lua_Entity_SetHP(lua_State* L) {
    CHECK_ENTITY(L);
    unsigned int hp = luaL_checkinteger(L, 2); // Second parameter is the new HP
    entity->SetBloody(hp);
    return 0; // No return value
}

// Lua binding: Take damage
// Lua绑定：受到伤害
static int lua_Entity_Damage(lua_State* L) {
    CHECK_ENTITY(L);
    unsigned int damage = luaL_checkinteger(L, 2);
    bool result = entity->Damage(damage);
    lua_pushboolean(L, result); // Return whether damage was successfully dealt
    return 1;
}

// Lua binding: Attack another entity
// Lua绑定：攻击其他实体
static int lua_Entity_Attack(lua_State* L) {
    CHECK_ENTITY(L);
    // Second parameter must be another Entity object
    // 第二个参数必须是另一个Entity对象
    Entity* target = *static_cast<Entity**>(luaL_checkudata(L, 2, "EntityMetaTable"));
    if (!target) {
        return luaL_error(L, "invalid target Entity");
    }
    bool result = entity->Attack(*target);
    lua_pushboolean(L, result);
    return 1;
}

// Lua binding: Set position
// Lua绑定：设置位置
static int lua_Entity_SetPosition(lua_State* L) {
    CHECK_ENTITY(L);
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    entity->SetPosition(x, y);
    return 0;
}

// Lua binding: Get position (returns Lua table {x=..., y=...})
// Lua绑定：获取位置（返回Lua表 {x=..., y=...}）
static int lua_Entity_GetPosition(lua_State* L) {
    CHECK_ENTITY(L);
    sf::Vector2f pos = entity->GetPosition();
    lua_newtable(L); // Create a table
    lua_pushnumber(L, pos.x);
    lua_setfield(L, -2, "x"); // table.x = pos.x
    lua_pushnumber(L, pos.y);
    lua_setfield(L, -2, "y"); // table.y = pos.y
    return 1; // Return the position table
}

// Lua binding: Move (based on Movement signal)
// Lua绑定：移动（根据Movement信号）
static int lua_Entity_Move(lua_State* L) {
    CHECK_ENTITY(L);
    // Assume Movement is an integer enum (e.g., 0=up, 1=down, etc.)
    // 假设Movement是整数枚举（如0=上，1=下等）
    int signal = luaL_checkinteger(L, 2);
    entity->Move(static_cast<atom::Movement>(signal));
    return 0;
}

// Register Entity metatable and methods to Lua environment
// 注册Entity元表和方法到Lua环境
auto RegisterEntityToLua(lua_State* L) -> void {
    // Create metatable (for identifying Entity type)
    // 创建元表（用于标识Entity类型）
    luaL_newmetatable(L, "EntityMetaTable");

    // The metatable's __index points to itself (for convenient method calls)
    // 元表的__index指向自身（方便调用方法）
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // Register Entity's Lua methods
    // 注册Entity的Lua方法
    const luaL_Reg entityMethods[] = {
        {"GetHP", lua_Entity_GetHP},
        {"SetHP", lua_Entity_SetHP},
        {"Damage", lua_Entity_Damage},
        {"Attack", lua_Entity_Attack},
        {"SetPosition", lua_Entity_SetPosition},
        {"GetPosition", lua_Entity_GetPosition},
        {"Move", lua_Entity_Move},
        {nullptr, nullptr} // End marker
    };
    luaL_setfuncs(L, entityMethods, 0);
	LOG_INFO(atom::LogChannel::ATOM_LUA, "Engine.Entity registered successfully.");
    // Pop metatable (clean up stack)
    // 弹出元表（清理栈）
    lua_pop(L, 1);
}

// Push a C++ Entity object into the Lua environment (as a global variable)
// 将C++的Entity对象推入Lua环境（作为全局变量）
auto PushEntityToLua(lua_State* L, Entity* entity, const std::string& luaVarName) -> void {
	if (!L || !entity) return;

	// 1. Create userdata to store the Entity pointer
	// 1. 创建userdata存储Entity指针
	// Allocate enough memory to store Entity*
	// 分配足够的内存存储Entity*
	Entity**udata = static_cast<Entity**>(lua_newuserdata(L, sizeof(Entity*)));
	*udata = entity; // Store the pointer

	// 2. Bind metatable (ensure Lua knows this is an Entity type)
	// 2. 绑定元表（确保Lua知道这是Entity类型）
	luaL_getmetatable(L, "EntityMetaTable");
	lua_setmetatable(L, -2);

	// 3. Set userdata as a Lua global variable
	// 3. 将userdata设置为Lua全局变量
	lua_setglobal(L, luaVarName.c_str());
}

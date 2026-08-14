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

#include "lua.hpp"

#include <Components/Entities/Entity.hpp>
#include <Log/LogSystem.hpp>

using Entity = atom::Entity;

#define CHECK_ENTITY(L)                                                                                                \
    Entity* entity = *static_cast<Entity**>(luaL_checkudata(L, 1, "EntityMetaTable"));                                 \
    if (!entity) {                                                                                                     \
        return luaL_error(L, "invalid Entity object");                                                                 \
    }

static int lua_Entity_GetHP(lua_State* L) {
    CHECK_ENTITY(L);
    lua_pushinteger(L, entity->GetHP());
    return 1;
}

static int lua_Entity_SetHP(lua_State* L) {
    CHECK_ENTITY(L);
    unsigned int hp = luaL_checkinteger(L, 2);
    entity->SetBloody(hp);
    return 0;
}

static int lua_Entity_Damage(lua_State* L) {
    CHECK_ENTITY(L);
    unsigned int damage = luaL_checkinteger(L, 2);
    bool result = entity->Damage(damage);
    lua_pushboolean(L, result);
    return 1;
}

static int lua_Entity_Attack(lua_State* L) {
    CHECK_ENTITY(L);
    Entity* target = *static_cast<Entity**>(luaL_checkudata(L, 2, "EntityMetaTable"));
    if (!target) {
        return luaL_error(L, "invalid target Entity");
    }
    bool result = entity->Attack(*target);
    lua_pushboolean(L, result);
    return 1;
}

static int lua_Entity_SetPosition(lua_State* L) {
    CHECK_ENTITY(L);
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    entity->SetPosition(x, y);
    return 0;
}

static int lua_Entity_GetPosition(lua_State* L) {
    CHECK_ENTITY(L);
    atom::Vec2 pos = entity->GetPosition();
    lua_newtable(L);
    lua_pushnumber(L, pos.GetX());
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, pos.GetY());
    lua_setfield(L, -2, "y");
    return 1;
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
    if (!L || !entity)
        return;

    // 1. Create userdata to store the Entity pointer
    // 1. 创建userdata存储Entity指针
    // Allocate enough memory to store Entity*
    // 分配足够的内存存储Entity*
    Entity** udata = static_cast<Entity**>(lua_newuserdata(L, sizeof(Entity*)));
    *udata = entity; // Store the pointer

    // 2. Bind metatable (ensure Lua knows this is an Entity type)
    // 2. 绑定元表（确保Lua知道这是Entity类型）
    luaL_getmetatable(L, "EntityMetaTable");
    lua_setmetatable(L, -2);

    // 3. Set userdata as a Lua global variable
    // 3. 将userdata设置为Lua全局变量
    lua_setglobal(L, luaVarName.c_str());
}

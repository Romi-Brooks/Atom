/**
  * @file           : LuaLoader.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/10/11
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_LUALOADER_HPP
#define ATOM_LUALOADER_HPP

// Standard Library
#include <string>
#include <unordered_map>

// Third Party Library
#include "lua.hpp"

// Engine Headers
#include <ECS/Components/Entities/Entity.hpp>

// Forward declarations for Lua binding registration functions
auto RegisterEntityToLua(lua_State* L) -> void;
auto RegisterMusicToLua(lua_State* L) -> void;
auto RegisterSFXToLua(lua_State* L) -> void;
auto RegisterVolumeToLua(lua_State* L) -> void;
auto PushEntityToLua(lua_State* L, atom::Entity* entity, const std::string& luaVarName) -> void;

// Set Lua bridge instances
// 设置 Lua 桥接实例
namespace atom {
class MusicPlayer;
class SFXPlayer;
class AudioMixer;
namespace audio {
class MusicCrossfade;
}
} // namespace atom
auto SetLuaMusicInstance(atom::MusicPlayer& music) -> void;
auto SetLuaMusicCrossfadeInstance(atom::audio::MusicCrossfade& transition) -> void;
auto SetLuaSFXInstance(atom::SFXPlayer& sfx) -> void;
auto SetLuaAudioMixerInstance(atom::AudioMixer& mixer) -> void;

class LuaLoader {
    private:
        // Lua state machine
        // Lua状态机
        lua_State* L_;
        // Records loaded scripts for hot-reload
        // 记录已加载的脚本，用于热重载
        std::unordered_map<std::string, std::string> loaded_scripts_;

        // Error handling
        // 错误处理
        auto HandleError(int result) const -> void;

    public:
        LuaLoader();
        ~LuaLoader();

        // Disallow copy to prevent duplicate Lua state machine release
        // 禁止拷贝，避免Lua状态机重复释放
        LuaLoader(const LuaLoader&) = delete;
        LuaLoader& operator=(const LuaLoader&) = delete;

        // Initialize Lua environment
        // 初始化Lua环境
        auto Initialize() -> bool;

        // Load and execute a Lua script
        // 加载并执行Lua脚本
        auto LoadScript(const std::string& scriptPath) -> bool;

        // Reload script (for hotfix)
        // 重新加载脚本（用于热修复）
        auto ReloadScript(const std::string& scriptPath) -> bool;

        // Call a global Lua function
        // 调用Lua中的全局函数
        auto CallLuaFunction(const std::string& funcName) const -> bool;

        // Register an Entity to the Lua environment
        // 注册Entity到Lua环境
        auto RegisterEntity(atom::Entity* entity, const std::string& luaVarName) const -> void;

        // Get the Lua state machine (use with caution; prefer encapsulated interfaces)
        // 获取Lua状态机（谨慎使用，尽量通过封装接口操作）
        [[nodiscard]] auto GetLuaState() const -> lua_State* {
            return L_;
        }
};

#endif // ATOM_LUALOADER_HPP

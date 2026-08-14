# LogSystem - 日志系统使用指引

[English](LogSystem.md) | [中文](LogSystem-CN.md)

***

## 概述

`LogSystem` 是 Atom 引擎的日志系统，支持分级日志输出，并通过 `LogChannel` 机制区分不同模块的日志来源。

---

## LogChannel 使用

`LogChannel` 允许您：

- **引擎内置通道** 以静态常量的形式提供（如 `LogChannel::ATOM_ENTITY`）
- **游戏自定义通道** 可以直接构造 `LogChannel` 实例，无需修改引擎源码

### 引擎内置通道

```cpp
// 直接使用，无需任何额外操作
LOG_INFO(atom::LogChannel::ATOM_ENTITY, "Entity created");
LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SFX not found");
LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Pack failed");
```

完整的内置通道列表：

| 通道常量 | 显示名称 |
|---|---|
| `ATOM_ENTITY` | Atom.Entity |
| `ATOM_ENTITY_NPC` | Atom.Entity.NPC |
| `ATOM_ENTITY_PLAYER` | Atom.Entity.Player |
| `ATOM_CONFIG_MOVEMENT` | Atom.Movement |
| `ATOM_FILESYSTEM` | Atom.Filesystem |
| `ATOM_MAIN` | Atom.Main |
| `ATOM_LUA` | Atom.Lua |
| `ATOM_AUDIO_MUSIC` | Atom.Audio.Music |
| `ATOM_AUDIO_SFX` | Atom.Audio.SFX |
| `ATOM_AUDIO_PLUG_MUSICFADE` | Atom.Audio.Plug.MusicFade |
| `ATOM_BACKEND_RUNTIME` | Atom.Backend.Runtime |
| `ATOM_VIDEO` | Atom.Video |
| `SDL_BACKEND_AUDIO` | SDL.Backend.Audio |
| `SDL_BACKEND_VIDEO` | SDL.Backend.Video |
| `SDL_BACKEND_RENDER` | SDL.Backend.Render |
| `SDL_BACKEND_WINDOW` | SDL.Backend.Window |
| `ATOM_WINDOW` | Atom.Window |
| `ATOM_SCREEN` | Atom.Screen |
| `ATOM_SCREEN_MANAGER` | Atom.Screen.Manager |
| `ATOM_UTILITIES_PACKAGER` | Atom.Utilities.Packager |

随着引擎的更新，对应的通道常量也会更新。

### 游戏自定义通道

不需要修改引擎代码，直接构造即可：


#### 定义为常量复用（推荐）

在游戏项目中创建自己的头文件：

```cpp
// 此文件在您的游戏项目中创建
#pragma once
#include "Log/LogSystem.hpp"

namespace game {
    const atom::LogChannel GAME_NPC("Game.NPC");
    const atom::LogChannel GAME_PLAYER("Game.Player");
    const atom::LogChannel GAME_SCREEN("Game.Screen");
    const atom::LogChannel GAME_MAIN("Game.Main");
}
```

使用：

```cpp
#include "GameLogChannels.hpp"

LOG_INFO(game::GAME_NPC, "NPC spawned");
LOG_ERROR(game::GAME_PLAYER, "Failed to save");
```

#### 临时使用（不推荐）

```cpp
LOG_INFO(atom::LogChannel("Game.NPC"), "NPC dialog started");
LOG_INFO(atom::LogChannel("Game.Player"), "Player save game");
```

显示效果：

```
[2026-01-01 12:00:00] [INFO] Game.NPC -> NPC dialog started
[2026-01-01 12:00:01] [INFO] Game.Player -> Player save game
```

通道名称会自动追加 ` -> ` 作为显示后缀。

---

## 日志级别

| 级别 | 宏 | 说明 |
|---|---|---|
| INFO | `LOG_INFO(channel, msg)` | 常规信息 |
| WARNING | `LOG_WARNING(channel, msg)` | 警告 |
| ERROR | `LOG_ERROR(channel, msg)` | 错误 |
| DEBUG | `LOG_DEBUG(channel, msg)` | 调试信息 |

示例：

```cpp
LOG_INFO(atom::LogChannel::ATOM_MAIN, "Engine started");
LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "File not found: " + filename);
LOG_ERROR(atom::LogChannel::ATOM_LUA, "Script error: " + errorMsg);
LOG_DEBUG(atom::LogChannel::ATOM_ENTITY, "Entity id: " + std::to_string(id));
```

---

## 设置日志显示级别

可以通过 `SetViewLogLevel` 控制只显示某个级别以上的日志：

```cpp
// 只显示 WARNING 及以上级别的日志
atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_WARNING);
```

级别优先级：`DEBUG < INFO < WARNING < ERROR`

---

## 注意事项

1. `LogChannel` 构造时接受 `std::string`，建议使用简短且有意义的名称
2. 通道名称是显示字符串，Atom 不会统一其大小写，因此应保持一致风格
3. 自定义通道不需要注册或提前声明，随用随建
4. 日志输出由内部 mutex 串行化；并发修改显示等级目前尚不保证线程安全

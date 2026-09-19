# LogSystem - 日志系统使用指引

[English](LogSystem.md) | [中文](LogSystem-CN.md)

***

## 概述

`LogSystem` 是 Atom 引擎的日志系统，支持分级日志输出，并通过**层级通道域**机制区分不同模块的日志来源。

每个域由一个宏（`ATOM_DEFINE_CHANNELS`）在编译期生成“枚举支持的值命名空间 + 名字映射 + 前缀”：

- **引擎域**：`atom::log::core`、`atom::log::audio`、`atom::log::entity`、`atom::log::render`、`atom::log::image`、`atom::log::layout`、`atom::log::debugger`、`atom::log::backend`、`atom::log::backend::Audio`、`atom::log::utilities` —— 输出前缀 `Atom.` / `Atom.Audio.` / `Atom.Render.` / ...
- **游戏域**：如 `game::log` —— 输出前缀 `Game.`（由游戏自己创建）

不需要任何运行时注册 —— `LOG_*` 宏会自动解析任意域的通道枚举（ADL）。显示前缀天然支持按层级筛选。

---

## 通道使用

### 引擎内置通道

定义在 `Log/AtomLogChannels.hpp`，按层级域组织：

```cpp
// 一级域：atom::log::core，前缀 "Atom."
ATOM_DEFINE_CHANNELS(atom::log::core, Channel, "Atom.",
    (Main, "Main"),
    (Logger, "Logger"),
    // ...
)

// 二级域：atom::log::audio，前缀 "Atom.Audio."
ATOM_DEFINE_CHANNELS(atom::log::audio, Channel, "Atom.Audio.",
    (Music, "Music"),
    (Sfx, "SFX"),
    // ...
)

// 三级域：atom::log::backend::Audio，前缀 "Atom.Backend.Audio."
// 通道值是具体后端 ID。
ATOM_DEFINE_CHANNELS(atom::log::backend::Audio, Channel, "Atom.Backend.Audio.",
    (sdl3, "SDL3"),
    (sdl3_mixer, "SDL3_mixer"),
    // ...
)
```

直接使用即可，自带 IDE 自动补全和编译期校验：

```cpp
LOG_INFO(atom::log::core::Main, "Engine started");
LOG_WARNING(atom::log::audio::Sfx, "SFX not found");
LOG_ERROR(atom::log::utilities::Packager, "Pack failed");
```

完整的引擎通道列表（按域分组）：

| 通道 | 显示名 |
|---|---|
| `atom::log::core::Main` | Atom.Main |
| `atom::log::core::Logger` | Atom.Logger |
| `atom::log::core::Filesystem` | Atom.Filesystem |
| `atom::log::core::Lua` | Atom.Lua |
| `atom::log::core::Video` | Atom.Video |
| `atom::log::core::Window` | Atom.Window |
| `atom::log::core::Screen` | Atom.Screen |
| `atom::log::core::ScreenManager` | Atom.Screen.Manager |
| `atom::log::core::Movement` | Atom.Movement |
| `atom::log::core::Entity` | Atom.Entity |
| `atom::log::entity::Npc` | Atom.Entity.NPC |
| `atom::log::entity::Player` | Atom.Entity.Player |
| `atom::log::audio::Music` | Atom.Audio.Music |
| `atom::log::audio::Sfx` | Atom.Audio.SFX |
| `atom::log::audio::PlugMusicFade` | Atom.Audio.Plug.MusicFade |
| `atom::log::audio::Minimp3` | Atom.Audio.Minimp3 |
| `atom::log::audio::WavProf` | Atom.Audio.WavProf |
| `atom::log::audio::SDL3Wav` | Atom.Audio.SDL3Wav |
| `atom::log::audio::Metadata` | Atom.Audio.Metadata |
| `atom::log::render::Renderer2D` | Atom.Render.Renderer2D |
| `atom::log::image::Decoder` | Atom.Image.Decoder |
| `atom::log::layout::Core` | Atom.Layout.Core |
| `atom::log::debugger::ImGui` | Atom.Debugger.ImGui |
| `atom::log::backend::Runtime` | Atom.Backend.Runtime |
| `atom::log::backend::Audio::sdl3` | Atom.Backend.Audio.SDL3 |
| `atom::log::backend::Audio::sdl3_mixer` | Atom.Backend.Audio.SDL3_mixer |
| `atom::log::backend::sdl3::Video` | Atom.SDL3.Backend.Video |
| `atom::log::backend::sdl3::Render` | Atom.SDL3.Backend.Render |
| `atom::log::backend::sdl3::Window` | Atom.SDL3.Backend.Window |
| `atom::log::utilities::Packager` | Atom.Utilities.Packager |

新增通道只需在 `Log/AtomLogChannels.hpp` 对应域添加一项 `(PascalCaseName, "Display.Name")`。每个域最多 64 个通道；域可以无限嵌套（如 `atom::log::entity::npc`）。后端实现通道是例外：使用运行时规范 ID，例如 `sdl3`、`sdl3_mixer`。

### 游戏自定义通道（独立域）

游戏侧完全不需要接触引擎源码。在游戏项目中创建一个头文件即可 —— **写通道 + 一行注入**：

```cpp
// Game/GameChannels.hpp —— 游戏侧全部代码
#pragma once
#include <Log/LogSystem.hpp>

// ============ 写通道 + 一行注入 ============
ATOM_DEFINE_CHANNELS(game::log, Channel, "Game.",
    (Npc, "NPC"),
    (Player, "Player"),
    (Main, "Main")
)
```

这一行宏调用在编译期生成 `game::log::Npc` 等直接值、名字映射和 `Game.` 前缀：

```cpp
#include "Game/GameChannels.hpp"

LOG_INFO(game::log::Npc, "NPC spawned");     // Game.NPC -> NPC spawned
LOG_ERROR(game::log::Player, "Save failed"); // Game.Player -> Save failed
```

也可以定义多个域（比如每个游戏模块一个，或嵌套如 `game::log::npc`），各自独立的命名空间、`Channel` 类型和前缀即可。

### 临时使用（Ad-hoc 通道）

快速试验时直接传字符串，无需任何声明：

```cpp
LOG_INFO("Game.NPC", "NPC dialog started");
LOG_INFO("Debug.temp", "Just trying something");
```

---

## 输出与层级筛选

每条日志都以域前缀开头，任意层级都能筛：

```
[2026-01-01 12:00:00] [INFO] Atom.Audio.Music -> Playback started
[2026-01-01 12:00:01] [INFO] Atom.Entity.NPC -> NPC spawned
[2026-01-01 12:00:02] [INFO] Game.NPC -> NPC spawned
[2026-01-01 12:00:03] [WARNING] Game.Player -> Save failed
```

- 过滤 `"Game."` → 只看游戏的日志
- 过滤 `"Atom.Audio."` → 查看音频业务域日志；`"Atom.Backend.Audio."` → 查看音频后端日志
- 过滤 `"Atom."` → 所有引擎日志

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
LOG_INFO(atom::log::core::Main, "Engine started");
LOG_WARNING(atom::log::audio::Sfx, "File not found: " + filename);
LOG_ERROR(atom::log::core::Lua, "Script error: " + errorMsg);
LOG_DEBUG(atom::log::core::Entity, "Entity id: " + std::to_string(id));
```

---

## 设置日志显示级别

可以通过 `SetViewLogLevel` 控制只显示某个级别以上的日志：

```cpp
// 只显示 WARNING 及以上级别的日志
atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_WARNING);
```

每次调用都会通过 `ATOM_LOGGER` 通道（`atom::log::core::Logger`）输出一条确认日志（例如
`Set log level to WARNING`），因此控制台中始终可以看到当前生效的日志级别。

级别优先级：`DEBUG < INFO < WARNING < ERROR`

---

## Windows 控制台 UTF-8

Windows 程序需要向控制台输出 UTF-8 文本时，在 `main` 的最开始、任何控制台或日志输出
之前调用一次：

```cpp
atom::Log::SetConsoleOutputUtf8();
```

Windows 上它封装 `SetConsoleOutputCP(CP_UTF8)`；其他平台为空操作。因此游戏代码不需要
平台条件编译，也不需要包含 `windows.h`。该设置只影响控制台输出，不改变控制台输入，
也不能替代终端中缺失的字体。

---

## TODO / 后续规划

日志也支持运行时订阅，调试 UI 可以通过 RAII connection 接收完整日志流：

```cpp
auto connection = atom::Log::Subscribe([](const atom::LogRecord& record) {
    // 不要在日志线程直接调用 ImGui；放入线程安全队列，在主线程消费。
});
```

`LogDebugger` 已使用这个接口，并独立于控制台的 `SetViewLogLevel` 进行过滤。
由 `ATOM_DEFINE_CHANNELS` 定义的引擎/游戏 channel 会自动注册到运行时列表，LogDebugger 使用下拉多选；临时字符串 channel 也会在首次出现后加入列表。

域筛选是下一个规划中的功能集

- [ ] **运行时按域过滤** —— 如 `atom::Log::SetChannelFilter("Atom.Audio.", false)`，按域（或单通道）静音/保留，无需 grep 即可把 `Game.` 日志与引擎噪音分开
- [ ] **按域设置显示级别** —— 例如 `Atom.Audio.` 保持 DEBUG，其余保持 WARNING
- [ ] **运行时通道配置文件**（可选）—— 从文件加载显示名/级别，游戏不改代码即可调整日志

---

## 注意事项

1. 通道是**枚举**——拼写错误在编译期就会报错，IDE 也能自动补全。
2. 显示格式为 `前缀 + 短名 + " -> "`。引擎域统一 `Atom.*` 前缀；游戏每个域自定义前缀。
3. 新增通道 = 在所属域的表单里加一行 `(CPP_NAME, "Short.Name")`。单个域最多 64 个通道；域可无限嵌套。
4. 临时通道直接用字符串，无需声明或注册。
5. 日志输出由内部 mutex 串行化；并发修改显示等级目前尚不保证线程安全。

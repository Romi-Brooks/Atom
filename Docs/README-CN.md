# Atom Engine

[English](../README.md) | [中文](README-CN.md)

***

**Atom** 是一个使用 **C++23** 编写、基于 **SDL3** 的模块化 **2D 游戏引擎**，旨在提供现代、清晰、轻量化的开发体验。

> **稳定版（master）** — 此分支跟踪最新的 Beta 版本。  
> **活跃开发在 [`dev`](https://github.com/Romi-Brooks/Atom/tree/dev) 分支进行。**  
> API 和架构可能会发生变化。欢迎反馈和贡献。

***

## 快速开始

### 前置依赖

- CMake >= 3.20
- 支持 C++23 的编译器
- 第三方依赖**随仓库分发**，位于 `ThirdParty/`

### 构建

```bash
git clone https://github.com/Romi-Brooks/Atom.git
cd Atom
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

***

## 示例

可直接运行的示例位于 [`Example/`](../Example/) 目录：

| 示例 | 说明 |
|------|------|
| `example_simple_window` | 最小 SDL3 窗口 |
| `example_simple_window_debug` | 带 ImGui 调试覆盖层的窗口 |
| `example_audio_playback` | 音乐播放 + 淡入淡出切换 |
| `example_sfx_playback` | 音效播放（Voice Pool 重叠播放） |
| `example_media_metadata` | 通过 TagLib 读取音频元数据 |

***

### 编码规范

请参阅完整的编码规范文档：

- [English](../CODING_STANDARD.md)
- [中文](CODING_STANDARD-CN.md)

***

## 资源打包工具

Atom 提供了资源打包工具，用于将游戏资源打包/解包为 HPKG 存档格式。

- [打包工具文档](../Utilities/Packager/Doc/README-CN.md) — CLI 使用、API 参考和代码示例

***

## 引擎依赖

Atom 使用 **SDL3** 作为其多媒体抽象层。SDL3 **随仓库分发**，位于 `ThirdParty/SDL3/`。

### 随仓库分发的依赖

| 库 | 版本 | 路径 | 用途 |
|----|------|------|------|
| [SDL3](https://github.com/libsdl-org/SDL) | 3.x | `ThirdParty/SDL3/` | 窗口管理、渲染、音频、输入 |
| [ImGui](https://github.com/ocornut/imgui) | 1.x | `ThirdParty/ImGUI/` | 调试覆盖层 UI |
| [Lua](https://www.lua.org/) | 5.x | `ThirdParty/Lua/` | 脚本引擎 |
| [TagLib](https://taglib.org/) | 2.x | `ThirdParty/taglib/` | 音频元数据读取 |
| [utfcpp](https://github.com/nemtrif/utfcpp) | 4.x | `ThirdParty/utfcpp/` | UTF-8 验证和编码转换 |

### SDL3 部署

SDL3 无编译器版本锁定——任何现代的 MinGW-w64 发行版（GCC 13+, UCRT）均可使用。JetBrains IDE 捆绑的 MinGW、WinLibs、MSYS2 等均可正常编译。

***

## 架构

```
Atom/
├── Engine/
│   ├── Audio/          # DecoderRegistry（解码器注册表）
│   ├── Interfaces/     # 抽象接口（IAudioDecoder, IAudioBuffer…）
│   └── Render/         # SDL3 窗口/渲染器封装
├── Media/
│   ├── Audio/
│   │   ├── Backend/    # SDL3 音频流源（Music, SFX, Buffer）
│   │   ├── Decoder/    # WAV 解码器 + AtomWavDecoderBackend
│   │   ├── Manager/    # VolumeManager, SFXManager
│   │   ├── Music/      # 音乐播放（领域层）
│   │   ├── Plugs/      # MusicFade 淡入淡出插件
│   │   └── SFX/        # 音效播放（Voice Pool 重叠支持）
│   └── Video/          # 视频（空壳）
├── Log/                # 日志系统
├── Window/             # 屏幕系统、Debugger
└── Lua/                # Lua 绑定
```

***

## 未来规划

- [x] **后端迁移** — 从 **SFML** 迁移到 **SDL3**（已完成）
- [ ] **渲染器插件系统** — 用户可以选择或编写自己的渲染后端
- [ ] **ECS 框架** — 将当前简单的实体系统替换为正式的 Entity-Component-System 架构
- [ ] **工具链** — 编辑器、资源管线和分析工具

***

### 致谢

Atom 引擎使用了以下开源库，衷心感谢这些项目的开发者：

- [SDL3](https://github.com/libsdl-org/SDL) — 窗口管理、渲染、音频和输入
- [ImGui](https://github.com/ocornut/imgui) — 调试覆盖层 UI
- [Lua](https://www.lua.org/) — 脚本引擎
- [TagLib](https://taglib.org/) — 音频元数据读取
- [utfcpp](https://github.com/nemtrif/utfcpp) — UTF-8 验证和编码转换

***

## 许可证
本项目使用 [MIT License](../LICENSE)

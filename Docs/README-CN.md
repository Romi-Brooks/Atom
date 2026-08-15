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
- Git，用于初始化 `ThirdParty/` 中锁定版本的源码依赖
- C 与 C++ 编译器；第三方依赖和 Atom 使用同一套工具链编译

### 构建

```bash
git clone --recurse-submodules https://github.com/Romi-Brooks/Atom.git
cd Atom
cmake -B build -G "MinGW Makefiles"
cmake --build build --parallel
```

项目在各平台统一使用标准 CMake 工作流。Windows 使用 MSVC + Ninja 时，请在
Visual Studio Developer Shell 中执行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Windows 使用 MinGW：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Linux 使用 GCC + Ninja：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
```

macOS 使用 Apple Clang 时可使用同样的 Ninja 命令，但无需指定 GCC。支持 CMake
的 IDE 可以直接配置仓库，无需维护 IDE 专用工程文件。

SDL3、TagLib、Dear ImGui、Lua、utfcpp 和 minimp3 都是固定 commit 的 Git
submodule。如果 clone 时没有取得依赖，请在配置 CMake 前执行
`git submodule update --init`。

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

Atom 提供了资源打包工具，用于将游戏资源打包/解包为 APKG 存档格式。

- [打包工具文档](../Utilities/Packager/Doc/Packager-CN.md) — CLI 使用、API 参考和代码示例

***

## 引擎依赖

Atom 使用 **SDL3** 作为其多媒体抽象层。所有第三方依赖都以源码
submodule 的形式锁定，并由 `ThirdParty/CMakeLists.txt` 在 Atom 相关目标之前编译。
各依赖的用途、上游 Git URL 和锁定 commit 记录在
[`ThirdParty/README.md`](../ThirdParty/README.md)。

### 源码依赖

| 库 | 版本 | 路径 | 用途 |
|----|------|------|------|
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.12 | `ThirdParty/SDL3/` | 窗口管理、渲染、音频、输入 |
| [ImGui](https://github.com/ocornut/imgui) | 1.92.9 | `ThirdParty/ImGUI/` | 调试覆盖层 UI |
| [Lua](https://www.lua.org/) | 5.4.7 | `ThirdParty/Lua/` | 脚本引擎 |
| [TagLib](https://taglib.org/) | 2.1.1 | `ThirdParty/taglib/` | 音频元数据读取 |
| [utfcpp](https://github.com/nemtrif/utfcpp) | 4.0.8 | `ThirdParty/utfcpp/` | UTF-8 验证和编码转换 |
| [minimp3](https://github.com/lieff/minimp3) | master `ea99364` | `ThirdParty/minimp3/` | MP3 解码（header-only） |

### 依赖编译

第三方依赖默认构建为静态库。首次干净构建耗时较长，后续构建会复用
CMake 和编译器产物。项目不再跨平台或跨编译器复用预编译库。

***

## 未来规划

- [未完成工作统一清单](Remaining-Issues.md)

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
- [minimp3](https://github.com/lieff/minimp3) — MP3 解码

***

## 许可证
本项目使用 [MIT License](../LICENSE)

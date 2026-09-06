# Atom Engine

[English](../README.md) | [中文](README-CN.md)

***

**Atom** 是一个使用 **C++23** 编写、基于 **SDL3 + SDL_GPU** 的模块化 **2D/3D 游戏引擎基础**，旨在提供现代、清晰、轻量化的开发体验。

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
- 提供 `glslc`、`spirv-val` 的 Vulkan Shader 工具（通过 `VULKAN_SDK` 或 `PATH` 发现）
- Windows 默认生成 D3D12 Shader，另需 DXC 和 `spirv-cross`
- macOS 默认生成 Metal Shader，另需 `spirv-cross`

Linux 默认只生成 SPIR-V，不需要 DXC。也可以使用 `ATOM_VULKAN_SDK_ROOT`、
`ATOM_DXC_ROOT` 提供仅保存在本机 CMake cache 中的查找提示；不得提交机器绝对路径。

### 构建

先获取仓库及固定版本的第三方依赖：

```bash
git clone --recurse-submodules https://github.com/Romi-Brooks/Atom.git
cd Atom
```

Windows（MSVC，请在 Visual Studio Developer PowerShell 中执行）：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Windows 使用 MinGW：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Linux（GCC + Ninja）：

先安装构建工具、Shader 工具和 SDL3 的系统开发库（Ubuntu 22.04+）：

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential git make cmake ninja-build pkg-config \
  glslc spirv-tools \
  libasound2-dev libpulse-dev libx11-dev libxext-dev \
  libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxtst-dev \
  libxss-dev libxkbcommon-dev libdrm-dev libgbm-dev \
  libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libdbus-1-dev \
  libfribidi-dev libibus-1.0-dev libthai-dev \
  libudev-dev libusb-1.0-0-dev libwayland-dev \
  wayland-protocols libdecor-0-dev
```

其中 `glslc` 用于把 GLSL 编译为 SPIR-V，`spirv-tools` 提供
`spirv-val`。其余开发包用于 SDL3 的 X11、Wayland 和音频功能；如果某个
可选驱动不可用，SDL3 会在配置时自动关闭该驱动。

配置并构建：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
```

SDL3、TagLib、Dear ImGui、Lua、utfcpp、minimp3 和 stb 都是固定 commit 的 Git
submodule。如果没有使用 `--recurse-submodules` 克隆，请在配置 CMake 前执行
`git submodule update --init`。

相关文档：

- [API 使用指南](API-Guide-CN.md)
- [未完成工作统一清单](Remaining-Issues.md)

***

## 示例

可直接运行的示例位于 [`Example/`](../Example/) 目录：

| 示例 | 说明 |
|------|------|
| `Example_Simple_Window` | 最小 SDL3 + SDL_GPU 窗口 |
| `Example_Simple_Window_Debug` | 基于 SDL_GPU 的 ImGui Debugger 覆盖层 |
| `Example_MusicCard` | Yoga + Renderer2D 音乐卡片及 ImGui 调试层；默认扫描 `E:\Music`，可传入目录参数 |
| `Example_Audio_Playback` | 音乐播放 + 淡入淡出切换 |
| `Example_SFX_Playback` | 音效播放（Voice Pool 重叠播放） |
| `Example_Audio_Metadata` | 通过引擎 AudioMetadataReader 读取音频标签与属性 |
| `Example_Packaged_Music` | 资源包打包 → 加载到内存 → 流式播放（MP3/WAV） |

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

Atom 使用 **SDL3** 作为其多媒体抽象层。第三方源码依赖以 submodule 形式固定版本，
并由 `ThirdParty/CMakeLists.txt` 在 Atom 相关目标之前编译。
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
| [stb](https://github.com/nothings/stb) | `2c980bb` | `ThirdParty/stb/` | 图片解码与可信字体栅格化 |

### 依赖编译

第三方依赖默认构建为静态库。首次干净构建耗时较长，后续构建会复用
CMake 和编译器产物。项目不再跨平台或跨编译器复用预编译库。

***

## 未来规划

- [x] **SDL_GPU 基础闭环** — SDL 自动选择 D3D12、Vulkan 或 Metal，已验证自定义 GLSL Shader 与基础 2D/3D。
- [x] **Renderer2D 与 Debugger 迁移** — 批处理 2D、图片/字体图集、MusicCard 与 ImGui 均已迁到 SDL_GPU；Atom target 不再构建 SDL_Renderer 路径。
- [ ] **原生 Vulkan 后端** — SDL_GPU 渲染器稳定后复用同一 RHI 与 SPIR-V 资产。
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
- [stb](https://github.com/nothings/stb) — 图片解码与可信字体栅格化

***

## 许可证
本项目使用 [MIT License](../LICENSE)

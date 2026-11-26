# Atom Engine

[English](../../README.md) | [中文](README-CN.md)

***

**Atom** 是一个使用 **C++23** 编写、基于 **SDL3 + SDL_GPU** 的模块化 **2D/3D 游戏引擎基础**，旨在提供现代、清晰、轻量化的开发体验。

> **稳定分支（master）** — 首个 Beta 版本正在准备中。
>
> **活跃开发在 [`dev`](https://github.com/Romi-Brooks/Atom/tree/dev) 分支进行。**
>
> API 和架构可能会发生变化。欢迎反馈和贡献。

***

## 快速开始

### 前置依赖

- CMake >= 3.20
- Ninja（使用下方 Ninja 命令时）
- Git，用于初始化 `ThirdParty/` 中锁定版本的源码依赖
- C 编译器和支持 C++23 的 C++ 编译器
- 提供 `glslc`、`spirv-val` 的 Vulkan Shader 工具（通过 `VULKAN_SDK` 或 `PATH` 发现）
- Windows 默认生成 D3D12 Shader，另需 DXC 和 `spirv-cross`
- macOS 默认生成 Metal Shader，另需 `spirv-cross`

Linux 默认只生成 SPIR-V，不需要 DXC。也可以使用 `ATOM_VULKAN_SDK_ROOT`、
`ATOM_DXC_ROOT` 指定工具位置。已有着色器产物时，可使用 `ATOM_PRECOMPILED_SHADER_DIR`。

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

先安装构建工具、Shader 工具和 SDL3 的系统开发库（Ubuntu）：

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
`spirv-val`。其余开发包用于 SDL3 的 X11、Wayland 和音频功能。如果某个
可选驱动不可用，SDL3 会在配置时自动关闭该驱动。

配置并构建：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
```

如果克隆时未初始化子模块，请在配置 CMake 前运行 `git submodule update --init --recursive`。

### 运行测试

默认构建 Atom 自身的测试。可通过 `ATOM_BUILD_TESTS=OFF` 关闭。

```bash
ctest --test-dir build --output-on-failure --timeout 90
```

### 目录与 API

`Engine/` 按领域组织运行时、平台、渲染、世界和资源等模块。`Example/` 提供可运行示例，
`Tools/` 存放独立工具，`Test/` 存放回归测试。应用按需链接模块目标，例如 `Atom_Runtime` 和 `Atom_World2D`。
公开头仍使用原有模块路径，例如 `<Runtime/Application.hpp>`。
应用可用 `atom::platform::ApplicationBasePath()` 定位资源，用 `JobSystem::ScheduleOrRun()`
提交无前置依赖的任务，用 `atom::random::Rng` 创建有种子的随机流。

- [API 使用指南](API-Guide-CN.md) — `Application` / `Screen`、输入、`UiTree`、音频与 ECS
- [未完成工作统一清单](../Issues/Remaining-Issues.md#9-beta-准备与里程碑) — Beta 里程碑和待办
- [子系统设计与实施记录](../Plans/Specs/Subsystem-Plans-CN.md)
- [场景 v2](../Plans/Specs/Scene-Schema-CN.md) · [精灵图集 v1](../Plans/Specs/Sprite-Atlas-CN.md) · [Tilemap](../Plans/Specs/Tilemap-Schema-CN.md) · [ECS 组件准入规则](../Plans/Specs/ECS-Component-Vocabulary-CN.md)

***

## 示例

可直接运行的示例位于 [`Example/`](../../Example/) 目录：

| 示例 | 说明 |
| --- | --- |
| `Example_Simple_Window` | 最小 SDL3 + SDL_GPU 窗口 |
| `Example_Simple_Window_Debug` | 带 ImGui 调试覆盖层的 SDL_GPU 窗口 |
| `Example_Input_Demo` | InputSystem、动作绑定与鼠标输入 |
| `Example_UI_Runtime_Demo` | 使用 `UiTree` 的保留式 UI 与页面切换 |
| `Example_Physics_Demo` | 交互式 2D 物理 |
| `Example_ECS_Tour` | 实体、组件存取与查询 |
| `Example_Job_System_Tour` | 任务提交、依赖与等待 |
| `Example_Lua_Scripting` | 加载 Lua 脚本并控制音频播放 |
| `Example_MusicCard` | 基于 Yoga 和 Renderer2D 的音乐播放器。可传入音乐目录，默认为 `E:\Music` |
| `Example_Audio_Playback` | 音乐播放与淡入淡出 |
| `Example_SFX_Playback` | 使用 Voice Pool 重叠播放音效 |
| `Example_Audio_Metadata` | 使用 `AudioMetadataReader` 读取音频标签和属性 |
| `Example_Packaged_Music` | 从 APKG 读取 MP3/WAV 条目到内存并播放 |
| `Example_Homeless` | [像素风可玩示例](../../Example/Homeless/README-CN.md)，展示 2D 相机、逐帧动画、粒子和音频 |

Release 构建可指定 `-DATOM_ENABLE_DEBUGGER=ON` 启用调试覆盖层。
Debug、RelWithDebInfo 和启用此选项的构建默认输出终端日志。
Windows 下，窗口示例在普通 Release 中启动时不弹出控制台。

***

### 编码规范

请参阅完整的编码规范文档。

- [English](../../CODING_STANDARD.md)
- [中文](CODING_STANDARD-CN.md)

***

## 资源打包工具

Atom Packager 读写 **APKG v2**，支持目录树、资源类型和内容校验。默认启动 GUI，
`--tui` 进入终端界面，`--path <package.apkg>` 在终端界面直接打开包。
脚本可使用 `pack`、`list` 和 `verify`。

引擎侧使用 `PackageBuilder` 创建资源包，通过 `Vfs::MountPackage` 挂载并读取内容。

- [打包工具文档](../../Engine/Atom/Resources/Utilities/Packager/Doc/Packager-CN.md) — 界面、命令行与 API

***

## 引擎依赖

Atom 使用 **SDL3** 作为其多媒体抽象层。第三方源码依赖以 submodule 形式固定版本，
并由 `ThirdParty/CMakeLists.txt` 在 Atom 相关目标之前编译。
各依赖的用途、上游 Git URL 和锁定 commit 记录在
[`ThirdParty/README.md`](../../ThirdParty/README.md)。

### 源码依赖

| 库 | 版本 | 路径 | 用途 |
| --- | --- | --- | --- |
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.12 | `ThirdParty/SDL3/` | 窗口、渲染、音频与输入 |
| [SDL_mixer](https://github.com/libsdl-org/SDL_mixer) | 3.2.4 | `ThirdParty/SDL_mixer/` | 混音，PCM 由 Atom 解码 |
| [ImGui](https://github.com/ocornut/imgui) | 1.92.9 | `ThirdParty/ImGUI/` | 调试覆盖层 UI |
| [Lua](https://www.lua.org/) | 5.4.7 | `ThirdParty/Lua/` | 脚本 |
| [TagLib](https://taglib.org/) | 2.1.1 | `ThirdParty/taglib/` | 音频元数据 |
| [utfcpp](https://github.com/nemtrif/utfcpp) | 4.0.8 | `ThirdParty/utfcpp/` | UTF-8 验证与转换 |
| [minimp3](https://github.com/lieff/minimp3) | `ea99364` | `ThirdParty/minimp3/` | MP3 解码 |
| [stb](https://github.com/nothings/stb) | `2c980bb` | `ThirdParty/stb/` | 图片解码 |
| [Yoga](https://github.com/facebook/yoga) | 3.2.1 | `ThirdParty/Yoga/` | Flexbox 布局 |
| [FreeType](https://github.com/freetype/freetype) | 2.13.3 | `ThirdParty/FreeType/` | 字体加载与字形栅格化 |
| [HarfBuzz](https://github.com/harfbuzz/harfbuzz) | 10.4.0 | `ThirdParty/HarfBuzz/` | OpenType 文本塑形 |
| [EnTT](https://github.com/skypjack/entt) | 4.0.0 | `ThirdParty/EnTT/` | 实体与组件存储 |

### 依赖编译

第三方依赖默认构建为静态库。首次干净构建耗时较长，后续构建会复用
CMake 和编译器产物。

***

## 未来规划

- [x] **SDL_GPU 基础闭环** — 支持 D3D12、Vulkan 和 Metal 的着色器产物，已有基础 2D/3D 绘制。
- [x] **Renderer2D 与 Debugger** — 精灵批绘制、字体图集、后处理和 ImGui 调试面板。
- [x] **ECS 与 2D 世界** — EnTT 存储、场景加载、图块地图、2D 物理、相机、逐帧动画和粒子。
- [x] **资源工具** — APKG v2 打包、检查和 VFS 挂载，提供 GUI 和终端模式。
- [ ] **首个 Beta SDK** — 动态库安装和 CMake 导出、运行时资源部署、跨平台外部项目验收。
- [ ] **原生 Vulkan 后端与更完整的工具链** — 见[剩余工作](../Issues/Remaining-Issues.md)。

***

### 致谢

感谢以下开源项目及其贡献者。

- [SDL3](https://github.com/libsdl-org/SDL) 和 [SDL_mixer](https://github.com/libsdl-org/SDL_mixer)
- [ImGui](https://github.com/ocornut/imgui) 和 [Yoga](https://github.com/facebook/yoga)
- [FreeType](https://github.com/freetype/freetype) 和 [HarfBuzz](https://github.com/harfbuzz/harfbuzz)
- [Lua](https://www.lua.org/) 和 [EnTT](https://github.com/skypjack/entt)
- [TagLib](https://taglib.org/)、[utfcpp](https://github.com/nemtrif/utfcpp)、[minimp3](https://github.com/lieff/minimp3) 和 [stb](https://github.com/nothings/stb)

***

## 许可证
本项目使用 [MIT License](../../LICENSE)

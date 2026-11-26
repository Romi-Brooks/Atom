# Atom SDK 打包与发布手册

> 状态：**实施稿 v4**（已核对仓库基线，分阶段验收）
> 适用版本：`0.1.0-prebeta`
> 基线：`dev` 分支 `c47a7ff`（2026-11-25），Windows MinGW Debug 构建与 CTest 81/81 通过。
> Windows 产物示例使用 MSVC 命名。MinGW 的静态库为 `libAtom.a`，导入库为 `*.dll.a`。
> 本文档是「讨论结论 + 统一口径的执行流程」。**每一步通过本阶段验收后再进入下一步。**
>
> **2026-11-26 变更**：按产品决定移除 `Test/SdkSmoke` 与 CI 冒烟步骤，发布前由发布负责人
> 用自己的游戏手动验证。文中关于 SdkSmoke 的段落保留为历史设计记录。

---

## 0. 阅读方式与术语

- 第 1~2 章：为什么做、概念扫盲（给第一次接触 shared library 的人）。
- 第 3~5 章：已经拍板的决策、目标架构、关键机制。
- 第 6~9 章：分阶段计划、测试、风险、发布检查表。
- 第 10 章：**全部问答归档**（谁问的、结论是什么）。
- 第 11 章：**统一口径的最终执行流程**（确认后按它执行）。

术语：

| 术语 | 含义 |
|---|---|
| 静态库（`.lib`/`.a`） | 链接时把代码复制进 exe；产物是单文件，但每个程序各有一份 |
| 动态库（`.dll`/`.so`） | 代码单独存放，运行时加载；多个程序共享一份 |
| 导入库（DLL 的 `.lib`） | 只含「符号在哪个 DLL」的接线图，不含代码 |
| 导出（export） | DLL 主动把符号列出来给外部用 |
| 导入（import） | 使用方声明「这个符号从 DLL 来」 |
| 组件（component） | `find_package(Atom COMPONENTS ...)` 里的一项，对应一个 DLL |

---

## 1. 背景、目标与非目标

### 1.1 为什么做 SDK

当前仓库是一个**自带可执行文件、静态链接**的引擎工程：示例和测试都在源码树里，用 `Engine/` 作为 include 根，靠 `Atom_All` / 各模块 target 链接。这适合开发，不适合交付：

- 用户不应该拿到源码树再自己编译引擎；
- 用户应该拿到 `include/` + `bin/` + `lib/`，像 SDL/SFML 一样 `find_package` 后直接写游戏；
- 引擎修 bug 时，用户只替换 DLL，不必重编游戏；
- 模块需要可选：做无窗口工具的用户不该被迫加载图形栈。

### 1.2 目标（可验收）

1. 用户解压 SDK 后，写一个实现 `atom::platform::ApplicationMain` 的 `.cpp`，用 `find_package(Atom)` 链接，能编译并运行。
2. 同一份用户代码，在 Windows x64 的 MSVC / MinGW-GCC / clang-cl 包下都能工作（每个包自包含，不跨工具链混用）。
3. 发布包在**干净的机器**上可用：没有隐藏的源码依赖，没有漏装的 DLL。
4. 引擎可以独立升级：替换 `bin/Atom.*.dll` 即可，用户 exe 不需要重编（ABI 不变的前提下）。
5. 包体与加载行为可解释：共享包的 `bin/` 里只有 Atom 的 DLL、`SDL3.dll` 和必要的运行时 DLL。

### 1.3 非目标（v1 明确不做）

- **C ABI / 任意编译器混用**：v1 仍是 C++ ABI，同一套 SDK 必须同工具链。
- **插件热加载 / 运行时模块发现**：DLL 只用于分发，不做插件系统。
- **全能力单头文件**：不做 `#include <Atom.hpp>` 这种伞形头；入口只有 `AtomMain.hpp`。
- **Linux/macOS 的正式发布包**：v1 先做 Windows x64；Linux 已在 CI 里构建验证，正式 `.so` 包放到 v2。

### 1.4 现状基线（已完成）

| 事项 | 状态 |
|---|---|
| `ApplicationConfig::fps` 启动限帧 | 已完成（`f7e7690`） |
| 测试/示例输出目录 `AtomTest/`、`AtomExample/` | 已完成（`a9ecf17`） |
| 删除无用的 `Engine/Atom.cpp` 与 `Atom_Engine` | 已完成（`c47a7ff`） |
| 单元测试 | 81/81 通过（Debug，Windows MinGW） |
| CI | `.github/workflows/sdk-release.yml`（Build Release SDK）：目标为 `master` 的 PR 构建并测试，`v*` 标签构建、测试、打包并发布 |

---

## 2. 基础概念（必读）

### 2.1 静态库、动态库、导入库

| | 静态库 | 动态库 |
|---|---|---|
| 代码在哪 | 链接时复制进 exe | 独立文件，运行时加载 |
| 分发 | 只发 exe | exe + DLL |
| 更新引擎 | 用户重编游戏 | 替换 DLL |
| 多游戏共用 | 不能 | 能（同一份内存代码） |
| 启动 | 一次到位 | 需要加载/重定位（毫秒级） |
| 调用开销 | 直接调用 | 间接跳转（纳秒级，可忽略） |

导入库是 DLL 的「接线图」：用户链接它，运行时才知道去 `Atom.Graphics.dll` 找函数。

### 2.2 共享 SDK 与静态 SDK 的区别（回答「有啥区别」）

| 维度 | 共享 SDK | 静态 SDK |
|---|---|---|
| 用户拿到 | `bin/*.dll` + `lib/*.lib` + `include/` | `lib/Atom.lib` + `lib/AtomEntry.lib` + `include/` |
| 用户 exe 体积 | 小 | 大（引擎代码全进去） |
| 部署 | 必须把 DLL 放到 exe 同目录 | 只需 exe |
| 引擎升级 | 替换 DLL 即可 | 必须重编游戏 |
| 多游戏共享内存代码 | 是 | 否，每个 exe 一份 |
| 插件/热加载 | 可行 | 很难 |
| 链接/调用开销 | 略高（可忽略） | 最低，可 LTO |
| ABI 约束 | 必须同工具链、同 Debug/Release | 同样必须同工具链（引擎静态库仍是该工具链编译的 C++ 目标文件） |
| 适用 | 正常游戏、长期维护 | 小工具、单文件分发、嵌入式场景 |

**本手册裁定**：v1 **两种都发，但分成两个独立压缩包**：

- `Atom-<ver>-<toolchain>-<config>-shared.zip`：DLL + 导入库 + 头 + CMake 包；
- `Atom-<ver>-<toolchain>-<config>-static.zip`：单个合并后的 `Atom.lib` + `AtomEntry.lib` + 头 + CMake 包，无 DLL。

**不要混用**：同一个包里同时放静态和共享 target 会让 `Atom::Core` 指向两个不同实现，用户一旦同时链就出现两份引擎状态。分两个包是最安全的做法，CI 也只是多一个构建目录。

静态包为什么是「一个 `Atom.lib`」：把各模块静态库和第三方静态库合并成一个归档，用户只需链一个库，且链接器仍会按需丢弃未引用对象。`Atom::Core` 等 component 在静态包里都是指向同一个 `Atom.lib` 的 interface target，用户 CMake 不用改。

### 2.3 导出与导入

DLL 默认只导出「列出来」的符号。MSVC 不写导出标记时，用户链接会看到 `unresolved external symbol`。

```cpp
// 编译 DLL 时
__declspec(dllexport) class RenderWindow { ... };
// 使用 DLL 时
__declspec(dllimport) class RenderWindow { ... };
```

用一个宏切换，具体见 5.1。GCC/Clang 在非 Windows 平台用 `visibility("default")` + 默认隐藏。

### 2.4 为什么 `main` 必须留在 exe

`main`（Windows GUI 是 `WinMain`）是进程起点，只能存在于 exe。引擎提供入口库，用户只实现业务入口：

```
my_game.exe
 ├─ main()/WinMain()   ← AtomEntry（引擎提供的静态库）
 ├─ ApplicationMain()  ← 用户实现
 └─ Atom.*.dll         ← 引擎
```

### 2.5 为什么 SDL3 要做成 DLL

SDL3 同时被窗口、GPU、音频使用。如果它被静态链接进多个 Atom DLL，进程里会出现多份 SDL 全局状态（初始化标志、事件队列、计时器），表现为「在一个模块里初始化，另一个模块里说没初始化」。

SDL3 做成 DLL 后，全进程只有一份，任何 Atom DLL 都可以安全使用。这也是大量商业游戏目录里出现 `SDL3.dll` 的原因。

### 2.6 为什么 STL 跨 DLL 需要同一工具链

`std::string` 的内存由运行时库（CRT）分配/释放；`std::vector` 的布局在 Debug/Release 下可能不同（MSVC 的 `_ITERATOR_DEBUG_LEVEL`）。DLL 与 exe 用不同工具链或不同配置时，会在边界处崩溃。

所以：**一套 SDK 一个工具链，Debug 对 Debug、Release 对 Release**。这是 SFML 等 C++ 引擎的通行规则，不是缺陷。

### 2.7 单例与头文件状态（审计结论）

规则：单例在头里声明、在**唯一一个** `.cpp` 里定义；公开头里不允许可变的 `inline` 变量或函数局部 `static`。

已对 `Engine/` 213 个公开头 + Example/Tools 19 个头做全量审计：

| 检查项 | 结果 |
|---|---|
| 12 个 `GetInstance` 单例 | 全部声明在头、定义在 `.cpp` ✅ |
| 可变的 `inline` 变量 | 0 ✅ |
| 命名空间级全局变量 / `thread_local` | 0 ✅ |
| 头文件内联函数里的可变 `static` | 0 ✅ |
| 类静态数据成员 | 全部 `constexpr`（不可变）✅ |

两处「无害但要知道」：

1. `Core/Log/LogChannelMacros.hpp` 生成的 `inline const bool channel_registered = []{ Register... }();`
   拆 DLL 后每个 DLL 都会执行一次；`RegisterLogChannelDomain` 按 `prefix+name` 去重，所以安全。**依赖「注册幂等」这一约定**，需在手册/测试中固定。
2. `Lua/LuaBinding/ContextBridge.hpp` 的 `static const LuaContext no_context{}`：`const` 兜底对象，不可达路径，安全。

CI 建议增加静态检查（见 5.7）。

### 2.8 `find_package` 是怎么工作的

安装时 CMake 生成 `AtomConfig.cmake` + `AtomTargets.cmake`，用户写：

```cmake
find_package(Atom CONFIG REQUIRED COMPONENTS Core Runtime Graphics)
target_link_libraries(my_game PRIVATE Atom::Graphics Atom::Runtime Atom::Entry)
```

`Atom::Graphics` 是导出时的逻辑名，指向 DLL 导入库 + 头文件路径 + 依赖项。**没有这两个生成文件，文档写得再好也无法 `find_package`。**

---

## 3. 决策记录（ADR）

| 编号 | 决策 | 理由 | 被否决的备选 |
|---|---|---|---|
| D01 | 源码树迁到 `Engine/Atom/...`，用户包含 `<Atom/...>` | 安装后 `include/` 是公共目录，`<Window/...>` 这类名字会和别的库撞车；开发/安装路径一致 | 安装时复制并改写 include（脆弱、双份真相） |
| D02 | 每个模块一个导出宏（`ATOM_CORE_API` 等） | 多 DLL 时同一个 `ATOM_EXPORTS` 会把别的模块符号也标成导出 | 全仓一个 `ATOM_API` |
| D03 | `SDL3.dll` 共享；其余第三方静态进各自唯一宿主 DLL | 一份 SDL 状态；其余第三方各只有一个模块使用，静态更省部署 | 所有第三方都做 DLL（DLL 过多） |
| D03a | **Lua 不单独出 DLL**：Lua 源码编进 `Atom.Scripting.dll`，并导出 Lua C API（`LUA_BUILD_AS_DLL`） | `LuaHost.hpp` 公开了 `lua_State*`/`BindingRegistrar`，用户写绑定需要 `lua_*` 符号。把 Lua 编进 Scripting 并导出 C API，Lua 实现只编译到一个 DLL，由各 LuaHost 管理自己的状态 | 单独 `lua.dll`（多一个文件）；或叫 `Atom.Lua.dll`（只是改名，仍需单独模块） |
| D04 | `Backend/Contracts` 放进 `Atom.Core.dll` | 契约是最底层；用户只链 Core 就能拿到接口，不需要理解「Contracts 是什么模块」 | 单独 `Atom.Contracts.dll`（多一个令人困惑的模块） |
| D05 | 第一版 10 个 DLL（见 4.3），**Debugger 必进** | 覆盖可选子系统 + 重第三方；不过度碎片化；调试能力是必须的 | 单 DLL（更新/可选性差）；按 30 个 target 全拆（DLL 太碎） |
| D06 | 粒度规则：整体可省略 / 绑重第三方 / 有独立生命周期，满足其一才单独成 DLL | 防止「拆得爽、维护哭」；拆分只是 CMake 工作，可后补 | 固定数量、按目录机械拆 |
| D07 | `AtomMain.hpp` 只声明入口；`AtomEntry` 静态库提供 `main`/`WinMain` | 入口在 exe；头不承担别的东西；不依赖 SDL_main | SDL 式 `#define main` 宏；把 `Application` 塞进入口头 |
| D08 | `find_package(Atom COMPONENTS ...)`，SFML 模型 | 用户按需链接，文档清晰；以后拆 DLL 不影响用户 CMake | 只给一个大 link 目标 |
| D09 | 版本 `0.1.0`，`Atom/Version.hpp` + `atom::Version()`，发布打 `v0.1.0-prebeta` | 支持问「你装的哪个版本」；便于发布管理 | 无版本号 |
| D10 | 共享包与静态包**各一个压缩包**：`bin/` + `lib/` + `include/` + `share/` | 分发简单；静态/共享不能混在一个包里 | 一个包同时放两种（`Atom::Core` 会指向两个实现） |
| D11 | 工具链矩阵：Windows x64 的 MSVC / MinGW-GCC / clang-cl 为 v1 正式包；Linux x64 的 GCC / Clang 在 CI 构建并出 experimental 包（v2 转正） | 覆盖用户主流环境；每个包自包含；Linux 尚未做完整人工验收 | 只支持 MSVC |
| D12 | v1 **同时发共享 SDK 与静态 SDK**，两个独立压缩包 | 共享用于正常游戏/长期维护；静态用于小工具/单文件分发；实现上只是多一个构建目录 | 只发共享（少了小工具场景）；一个包混发（会重复引擎） |
| D13 | 删除 `Atom_Engine` 与 `Engine/Atom.cpp` | 只打印 banner，不测入口/运行时，对 SDK 无用 | 改造成冒烟程序（用户决定不需要） |
| D14 | 不做全能力头文件 | 编译成本、依赖泄漏、层级纪律 | `Atom.hpp` 包含一切 |
| D15 | 不导出 `Atom_All` / `Atom_Core` 兼容包到 SDK | 避免用户「一把梭」全链，编译/体积失控 | 导出便利包 |
| D16 | 只安装 public 头文件（排除后端实现、内部组合层；SDL 头不安装；ImGui/Lua 头作为受控第三方随包提供） | 防止用户依赖 SDL/内部实现；ImGui/Lua 是公开调试与脚本能力所必需 | 整个 `Engine/` 原样安装 |
| D17 | ABI：同工具链、同 Debug/Release；每个包自包含 | C++ ABI 现实；与 SFML 一致 | 承诺任意编译器（需要 C facade） |
| D18 | `Atom.Debugger.dll` **必进 v1**，公开 `Overlay.hpp` 等头，用户可写自定义 ImGui 面板 | 调试能力对用户很重要；ImGui 只编进 Debugger DLL 并从中导出 API，保证全进程一个 ImGui 上下文 | 不公开自定义面板（能力缺失）；单独 imgui.dll（多一个文件，非必需） |
| D19 | CI 用 GitHub Actions 出包 | 已有 Windows/Linux 构建流水线，扩展即可 | 本地手工打包 |
| D20 | 测试/示例输出到 `AtomTest/`、`AtomExample/` | 构建目录可读；已落地 | 全部堆在构建根目录 |

---

## 4. 目标架构

### 4.1 源码树（Phase 1 之后）

```
Engine/
  Atom/
    AtomMain.hpp              # 唯一入口声明
    Build/                    # 每模块 Export.hpp（或放在各模块下）
    Core/                     # Error/Log/Math/Random/UTF8/Time/Threading/Concurrency/FS/Json/Assets/ECS/Sprite/InputSystem
    Backend/
      Contracts/              # 接口（随 Core 一起导出）
      Extension/              # 注册表（内部）
      Composition/            # 运行时组合（内部）
      SDL3/ SDLGPU/           # 实现（不安装）
    Platform/                 # Window/Display/Input/ApplicationPaths/FileDialogs
    World/                    # Physics/Scene（不含 ECS：ECS 在 Core）
    Runtime/                  # Application/ScreenManager/GameWorld
    Presentation/             # Renderer2D/Text/UI/Layout/Scene2D/World2D
    Resources/                # FS/Asset/Media(音频/图片)/Packager
    Platform/Entry/           # ConsoleMain.cpp / WindowsMain.cpp
    Lua/                      # Scripting + 绑定（Lua 编入 Scripting DLL 并导出 C API）
    Debugger/                 # 面板（Overlay/FontLoader 公开，ImGui API 从 Debugger DLL 导出）
```

include 根目录统一为 `Engine/`，所有引擎头一律 `<Atom/...>`。

### 4.2 安装树

共享包：

```
Atom-0.1.0-msvc-x64-Release-shared/
  bin/
    Atom.Core.dll
    Atom.World.dll
    Atom.Backends.dll
    Atom.Platform.dll
    Atom.Runtime.dll
    Atom.Graphics.dll
    Atom.Media.Audio.dll
    Atom.Media.Image.dll
    Atom.Scripting.dll         # 内含并导出 Lua C API
    Atom.Debugger.dll          # 内含并导出 ImGui API
    SDL3.dll
    libstdc++-6.dll            # MinGW 实际依赖的编译器运行时
    libgcc_s_seh-1.dll         # 同上
    libwinpthread-1.dll        # 同上
  lib/
    Atom.*.lib                 # 导入库
    AtomEntry.lib              # 入口（静态）
    AtomEntryWindowed.lib
  include/
    Atom/
      AtomMain.hpp
      Core/... Backend/Contracts/... Platform/... Runtime/...
      Presentation/... Resources/... Lua/... Debugger/...
    imgui/                     # imgui.h / imgui_internal.h / imconfig.h 等
    lua/                       # lua.h / lualib.h / lauxlib.h / luaconf.h
  share/Atom/cmake/
    AtomConfig.cmake
    AtomTargets.cmake
    AtomHelpers.cmake          # atom_application()
  README.md
  LICENSE
```

静态包：

```
Atom-0.1.0-msvc-x64-Release-static/
  lib/
    Atom.lib                   # 合并后的引擎 + 第三方静态库（单库）
    AtomEntry.lib
    AtomEntryWindowed.lib
  include/                     # 与共享包相同
  share/Atom/cmake/            # 与共享包相同，Atom::* 全部指向 Atom.lib
  README.md
  LICENSE
```

> 静态包没有 `bin/`；用户只需 `Atom.lib` + 系统库（如 `user32`/`gdi32`/`ws2_32`，
> 由 `AtomConfig.cmake` 的 `INTERFACE_LINK_LIBRARIES` 自动带上）。

### 4.3 DLL 划分与依赖

第一版 10 个（`Atom.Debugger` **必进**）：

> 归属按「现有 target 的真实依赖」定，不按目录名想当然。关键三条：
> 1. `Atom_World2D` 依赖 `Atom_Scene2D`（Presentation），所以它不能留在 World，否则
>    World → Graphics 与 Graphics → ECS 会形成环；本手册把 World2D 放进 Graphics
>    （2D 栈），以后可再单独拆 `Atom.World2D.dll`。
> 2. `Atom_Scene` 依赖 `Atom_Sprite`，而 Sprite 只依赖 Json/FS/Math/Color，是**纯资产
>    元数据**，因此 Sprite 与 ECS 一起放进 Core，World 只保留 Physics/Scene。
> 3. `Atom_Backend_Composition` 依赖 `Atom_Audio_Codecs`，而 Media 又依赖 Backends；
>    为避免环，Audio_Codecs 放进 Backends。
> 4. World2D 通过 Core 的 ECS 组件描述碰撞体。物理系统仍由 World 提供。
> 5. SDL_GPU 的 ImGui 工厂由 Debugger 在初始化 overlay 时注册。Backends 提供注册表和渲染实现，
>    保持可独立加载。Debugger 独占 ImGui 和其 SDL_GPU glue，调用所需的 Backends 入口。
> 6. Platform 的应用路径与文件对话框直接调用 SDL3，SDL 头留在实现文件内。

| DLL | 包含（target） | 私有第三方 | 依赖 |
|---|---|---|---|
| `Atom.Core.dll` | Error、Log、Math/Algorithm/Color、Random、UTF8、Time、Threading/Concurrency、FS、Json、Assets、Packager、**ECS**、**Sprite**、**InputSystem**、**Backend/Contracts** | EnTT | 无 |
| `Atom.World.dll` | Physics、PhysicsIntegration、Scene | 无 | Core |
| `Atom.Backends.dll` | Backend/Extension、Composition、SDL3 Core/Window/Audio/Mixer、SDL_GPU、Audio_Codecs | SDL3（动态）、SDL3_mixer（静态）、minimp3 | Core、SDL3 |
| `Atom.Platform.dll` | Window、Display、ApplicationPaths、FileDialogs | SDL3（动态） | Core、Backends、SDL3 |
| `Atom.Runtime.dll` | Runtime（Application、ScreenManager、GameWorld、调度器） | 无 | Core、Platform |
| `Atom.Graphics.dll` | Render_Core、Render_Pipeline、Renderer2D、Render_Resources、Text、UI、Layout、Scene2D、**World2D** | FreeType、HarfBuzz、Yoga | Core、Media.Image |
| `Atom.Media.Audio.dll` | Media（播放器/混音/解码调度） | TagLib | Core、Backends |
| `Atom.Media.Image.dll` | Image | stb_image | Core |
| `Atom.Scripting.dll` | Lua、LuaBinding | 无（Lua 编入本 DLL 并导出 C API） | Core、Media.Audio |
| `Atom.Debugger.dll` | Debugger、Backend_SDLGPU_Debug | ImGui（编入本 DLL 并导出 API） | Core、Platform、Runtime、Backends、SDL3 |

依赖图（箭头 = 依赖，禁止反向/环）：

```
Core  (含 ECS / Sprite / InputSystem / Backend Contracts)
 ├── World            (Physics / Scene)
 ├── Backends         (SDL3 / SDL_GPU / 音频后端 / 解码器)
 │    ├── Platform    (Window / Display / Paths / FileDialogs)
 │    │    └── Runtime
 │    ├── Media.Audio
 ├── Graphics         (Renderer2D / Text / UI / Layout / Scene2D / World2D)
 │    └── 也依赖 Media.Image
 ├── Media.Image
 └── Scripting       (依赖 Media.Audio)
Debugger → Core + Platform + Runtime + Backends + SDL3
```

禁止反向依赖与环。`Atom.Debugger` 在包里始终存在，但游戏可以按需决定是否链接它。
`World` 不依赖 Graphics，所以「只要物理/场景、不要 GPU」的用户可以只链 Core + World。

### 4.3.1 第三方 API 的导出（Lua / ImGui）

这两个第三方是**公开能力**的一部分，但都只编进一个 DLL，并从该 DLL 导出 API：

| 第三方 | 宿主 DLL | 构建时 | 消费方（用户） |
|---|---|---|---|
| Lua | `Atom.Scripting.dll` | 编译 Lua 源码时定义 `LUA_BUILD_AS_DLL`（源文件已有 `LUA_CORE` 或 `LUA_LIB`） → `LUA_API` = `dllexport` | `Atom::Scripting` 的 INTERFACE 定义 `LUA_BUILD_AS_DLL`（不定义 CORE/LIB）→ `LUA_API` = `dllimport`；链 `Atom::Scripting` 即可调用 `lua_*` |
| ImGui | `Atom.Debugger.dll` | 编译 ImGui 源码 + `IMGUI_API=__declspec(dllexport)` | `Atom::Debugger` 的 INTERFACE 定义 `IMGUI_API=__declspec(dllimport)`；`#include <imgui.h>` + 链 `Atom::Debugger` |

为什么这样做：

- 全进程加载一份 Lua 实现，各 LuaHost 管理自己的 Lua 状态。Atom 调试面板共用引擎的 ImGui context。
- 用户无需额外链接 `lua.lib` / `imgui.lib`，也不会出现「DLL 里一个 context、exe 里一个 context」；
- 代价：**用户不能同时再链接另一份 Lua/ImGui**（文档写明）；Linux 上把相应 DLL 的符号可见性设为 default。

> 如果你更希望看到一个独立文件，Lua 也可以构建成 `Atom.Lua.dll`（改 `OUTPUT_NAME` 即可），
> 但那样 `Atom.Scripting.dll` 还要依赖它，收益不大；ImGui 同理可选 `Atom.ImGui.dll`。

**未来方向（不在 v1）**：可以逐步把 ImGui 封装成 Atom 自己的模块（例如 `Atom.ImGui.dll` +
Atom 风格的 `Panel`/`Widget` 包装层），这样用户不必直接面对原始 ImGui 头，升级 ImGui 也由
引擎负责。但 v1 保留原始 ImGui 头 + 从 `Atom.Debugger.dll` 导出，给用户最大自由度；封装层
以后可以叠加在这套机制之上（导出不变，只是再加一层 Atom API），不会推翻现在的设计。

### 4.4 public / internal 头文件边界

**安装**：`Engine/Atom/` 下除下列排除项外的全部 `.hpp`。

**排除（internal）**：

- `Atom/Backend/SDL3/**`、`Atom/Backend/SDLGPU/**`（原生后端实现）
- `Atom/Backend/Composition/**`、`Atom/Backend/Extension/**`（注册表/组合内部）
- `Atom/Platform/Entry/**`（入口实现）
- 任何包含禁用第三方头的头文件（用 5.7 的检查兜底）

音频公开 API 使用 `AudioDecoderRegistry` 和 `IAudioBackendChangeListener`。S4 要将这两个声明移到可安装路径，保持类型和命名空间，并继续由原 DLL 宿主提供实现。安装验收从独立目录编译每个头文件，检查其依赖能在安装前缀内解析。

**第三方头（受控安装，不算 public API）**：

- `Atom/Lua/**` 全部安装，并附带 `lua.h/lualib.h/lauxlib.h/luaconf.h`（D03a）；
- `Atom/Debugger/**` 全部安装（含 `Overlay.hpp`、`FontLoader.hpp`），并附带
  `imgui.h`、`imgui_internal.h`、`imconfig.h`、`imstb_*` 等 ImGui 核心头（D18）；
- SDL3 头**不安装**（后端是 internal，用户不需要）。

### 4.5 用户视角

```cpp
// main.cpp
#include <Atom/AtomMain.hpp>
#include <Atom/Platform/Window/RenderWindow.hpp>
#include <Atom/Runtime/Application.hpp>
#include <Atom/Runtime/ScreenManager.hpp>
#include <Atom/Runtime/Screen.hpp>
#include <Atom/Core/Log/LogSystem.hpp>

class GameScreen final : public atom::Screen { ... };

auto atom::platform::ApplicationMain(int, char**) -> int {
    auto& app = atom::runtime::Application::GetInstance();
    atom::runtime::ApplicationConfig config{};
    config.title = "My Game";
    if (const auto started = app.Initialize(config); !started) return 1;
    // ... LoadScreen / SwitchScreen
    app.Run();
    return 0;
}
```

```cmake
find_package(Atom 0.1 CONFIG REQUIRED COMPONENTS Core Runtime Graphics)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE Atom::Runtime Atom::Graphics)
atom_application(my_game WINDOWED)   # 由 AtomHelpers.cmake 提供；内部链 Atom::Entry*
```

（`atom_application()` 负责入口库；不需要再手动链 `Atom::Entry`。如果你自己实现 `main`，可以不用这个 helper、也不链入口库。）

自定义调试面板（v1 支持，D18）：

```cpp
#include <Atom/Debugger/Overlay.hpp>   // 引入 ImGui 与 Overlay 入口
#include <imgui.h>

class MyPanel final : public atom::debugger::DebugPanel {
    public:
        MyPanel() : DebugPanel("My Panel") {}
    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Text("Hello from a custom panel");
        }
};
```

```cmake
target_link_libraries(my_game PRIVATE Atom::Debugger)
```

ImGui 的符号从 `Atom.Debugger.dll` 导出，所以**不要**再链接另一份 ImGui 静态库/源码；
`Atom::Debugger` 已经带上 `imgui.h` 的 include 路径和 `IMGUI_API` 的导入定义。

---

## 5. 关键机制

### 5.1 每模块导出宏

```cpp
// Engine/Atom/Core/Export.hpp
#ifndef ATOM_CORE_EXPORT_HPP
#define ATOM_CORE_EXPORT_HPP

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(ATOM_BUILD_SHARED)
#    if defined(ATOM_CORE_EXPORTS)
#      define ATOM_CORE_API __declspec(dllexport)
#    else
#      define ATOM_CORE_API __declspec(dllimport)
#    endif
#  else
#    define ATOM_CORE_API
#  endif
#else
#  if defined(ATOM_BUILD_SHARED)
#    define ATOM_CORE_API __attribute__((visibility("default")))
#  else
#    define ATOM_CORE_API
#  endif
#endif

#endif // ATOM_CORE_EXPORT_HPP
```

其他模块照抄，替换 `CORE`。用法：

```cpp
class ATOM_CORE_API Vfs { ... };
ATOM_CORE_API auto ApplicationBasePath() -> Result<std::filesystem::path>;
```

CMake：

```cmake
set(ATOM_LIBRARY_TYPE STATIC)
if(ATOM_BUILD_SHARED)
    set(ATOM_LIBRARY_TYPE SHARED)
endif()

add_library(Atom_Core ${ATOM_LIBRARY_TYPE} ...)
target_compile_definitions(Atom_Core PRIVATE ATOM_CORE_EXPORTS)
target_compile_definitions(Atom_Core PUBLIC $<$<BOOL:${ATOM_BUILD_SHARED}>:ATOM_BUILD_SHARED>)
if(ATOM_BUILD_SHARED)
    set_target_properties(Atom_Core PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()
```

注意：

- 头文件里的 `inline` 函数只能调用已导出的符号，否则消费方链接失败；
- 普通模板保持头内实现。ECS 公开模板调用的后端显式实例由 Core 导出，EnTT 继续留在实现文件内；
- `ATOM_BUILD_SHARED` 必须作为 **INTERFACE** 定义传给消费者，否则用户看到的是 `dllimport` 缺失的状态。

### 5.2 入口：`AtomMain.hpp` + `AtomEntry`

```cpp
// Engine/Atom/AtomMain.hpp
#ifndef ATOM_ATOM_MAIN_HPP
#define ATOM_ATOM_MAIN_HPP

namespace atom::platform {
// 在可执行文件里实现；AtomEntry 提供 main/WinMain 并转发参数与退出码。
auto ApplicationMain(int argc, char** argv) -> int;
}

#endif // ATOM_ATOM_MAIN_HPP
```

入口库（静态，链进 exe，不进 DLL）：

```cpp
// Engine/Atom/Platform/Entry/ConsoleMain.cpp
#include <Atom/AtomMain.hpp>
auto main(int argc, char** argv) -> int {
    return atom::platform::ApplicationMain(argc, argv);
}
```

```cpp
// Engine/Atom/Platform/Entry/WindowsMain.cpp（Windows GUI）
#include <Atom/AtomMain.hpp>
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

namespace {
// 用 GetCommandLineW + CommandLineToArgvW 自己解析，再转 UTF-8；
// 不依赖 SDL_main，也不用 -municode。
auto WideArguments() -> std::vector<std::string> { /* ... */ }
}

auto WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) -> int {
    auto arguments = WideArguments();
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    return atom::platform::ApplicationMain(static_cast<int>(arguments.size()), argv.data());
}
```

要点：

- 用 `WinMain`（不是 `wWinMain`），避免 MinGW 需要 `-municode`；GUI 命令行用
  `GetCommandLineW` 解析，转换为以空指针结尾的 UTF-8 `argv`。控制台转发现有 CRT 参数。
- 入口库**不链接 SDL**；Backends、Platform 和 Debugger 共用 `SDL3.dll`。
- 控制台应用链接 `AtomEntry`，GUI 应用链接 `AtomEntryWindowed`；CMake helper 选择。
- 用户永远不写 `main`，只写 `ApplicationMain`。

### 5.3 `atom_application()` helper（随 SDK 导出）

```cmake
# share/Atom/cmake/AtomHelpers.cmake
function(atom_application target mode)
    if(NOT mode STREQUAL "WINDOWED" AND NOT mode STREQUAL "CONSOLE")
        message(FATAL_ERROR "atom_application(${target}) requires WINDOWED or CONSOLE")
    endif()
    if(WIN32 AND mode STREQUAL "WINDOWED")
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_libraries(${target} PRIVATE Atom::EntryWindowed)
    else()
        target_link_libraries(${target} PRIVATE Atom::Entry)
    endif()
endfunction()
```

### 5.4 安装与 `AtomConfig.cmake`

```cmake
# 每个 target 设置导出名（否则用户会看到 Atom::Atom_Core）
set_target_properties(Atom_Core PROPERTIES EXPORT_NAME Core)
set_target_properties(Atom_Runtime PROPERTIES EXPORT_NAME Runtime)
# ... 其余同理

install(TARGETS ${ATOM_SDK_TARGETS}
    EXPORT AtomTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/Engine/Atom/"
    DESTINATION include/Atom
    FILES_MATCHING PATTERN "*.hpp"
    PATTERN "Backend/SDL3" EXCLUDE
    PATTERN "Backend/SDLGPU" EXCLUDE
    PATTERN "Backend/Composition" EXCLUDE
    PATTERN "Backend/Extension" EXCLUDE
    PATTERN "Platform/Entry" EXCLUDE)

# 第三方头：Debugger 的自定义面板需要 imgui.h；Scripting 的绑定需要 lua 头
install(DIRECTORY "${PROJECT_SOURCE_DIR}/ThirdParty/ImGUI"
    DESTINATION include/imgui
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/ThirdParty/Lua"
    DESTINATION include/lua
    FILES_MATCHING PATTERN "*.h")

# 让 Atom::Debugger / Atom::Scripting 带上第三方头的 include 路径
target_include_directories(Atom_Debugger INTERFACE
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/imgui>)
target_include_directories(Atom_Scripting INTERFACE
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/lua>)

install(EXPORT AtomTargets
    NAMESPACE Atom::
    FILE AtomTargets.cmake
    DESTINATION share/Atom/cmake)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/AtomConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AtomConfig.cmake"
    INSTALL_DESTINATION share/Atom/cmake)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AtomConfig.cmake"
    DESTINATION share/Atom/cmake)
install(FILES "${PROJECT_SOURCE_DIR}/cmake/AtomHelpers.cmake"
    DESTINATION share/Atom/cmake)

# 没有这个版本文件，find_package(Atom 0.1) 会直接失败
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AtomConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AtomConfigVersion.cmake"
    DESTINATION share/Atom/cmake)
```

第三方 API 的导出定义（Windows；Linux 用 `visibility("default")` 的等价写法）：

```cmake
if(ATOM_BUILD_SHARED)
    # Lua：编进 Atom.Scripting.dll，见 4.3.1
    target_compile_definitions(Atom_Scripting PRIVATE
        LUA_BUILD_AS_DLL)
    target_compile_definitions(Atom_Scripting INTERFACE LUA_BUILD_AS_DLL)

    # ImGui：编进 Atom.Debugger.dll，见 4.3.1
    target_compile_definitions(Atom_Debugger PRIVATE IMGUI_API=__declspec(dllexport))
    target_compile_definitions(Atom_Debugger INTERFACE IMGUI_API=__declspec(dllimport))
endif()
```

> 前提：Lua/ImGui 的编译单元必须带 `dllexport` 定义。可以像现在这样保留
> `Atom_3rd_Lua` / `Atom_3rd_ImGui` 静态库，但在 SHARED 构建时给这两个 target 加
> `LUA_BUILD_AS_DLL` 或 `IMGUI_API=__declspec(dllexport)`；
> 也可以直接把源码并进 `Atom_Scripting` / `Atom_Debugger`。**不能**用一个没带导出定义的
> 静态库链进 DLL——那样符号不会被导出。静态 SDK 下这些定义不启用，`LUA_API`/`IMGUI_API`
> 为空，行为与现在一致。

#### 静态 SDK 的安装

静态包用同一份 CMake，只是 `ATOM_BUILD_SHARED=OFF`、单独一个 build 目录与安装前缀：

```powershell
cmake -S . -B build-static -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-static --target AtomSdkArchive --parallel
cmake --install build-static --prefix _sdk-static
```

合并用一个打包脚本（MSVC 用 `lib.exe /OUT:Atom.lib *.lib`；MinGW 用 `ar -M`）。
合并后 `Atom::Core` 等 component 在 `AtomConfig.cmake` 里都指向同一个 `Atom.lib`，
用户链接方式不变。

`AtomConfig.cmake.in`：

```cmake
@PACKAGE_INIT@
include("${CMAKE_CURRENT_LIST_DIR}/AtomTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/AtomHelpers.cmake")

# 组件可用性：每个导出的 target 对应一个 component
foreach(component IN ITEMS Core World Backends Platform Runtime Graphics
                       Media Audio Image Scripting Debugger Entry EntryWindowed)
    if(TARGET Atom::${component})
        set(Atom_${component}_FOUND TRUE)
    endif()
endforeach()

check_required_components(Atom)
```

DLL 运行时：`bin/` 与用户 exe 同目录即可；Windows 从 exe 目录搜索 DLL。

### 5.5 include 前缀迁移方法

现状不是「统一以 Engine 为根」：不同模块 target 的 include 根不同（`Engine/Core`、
`Engine/Resources`、`Engine/World`、`Engine/Presentation`、`Engine/Platform`，其余以
`Engine` 为根）。所以迁移是「先归一到 Engine 根，再统一加 `Atom/` 前缀」。

#### 5.5.1 为什么不能盲加前缀（重点，代码层风险）

这一步唯一容易做错的地方，是**用字符串替换去猜文件位置**。下面三组例子说明为什么不行：

```cpp
// 例 1：同一个 Utilities/，两个不同模块
#include <Utilities/Json/JsonReader.hpp>          // 实际：Engine/Core/Utilities/Json/
#include <Utilities/Packager/PackageBuilder.hpp>   // 实际：Engine/Resources/Utilities/Packager/
// 盲替换 <Utilities/ → <Atom/Utilities/ 会把两行指到同一个地方，至少一边编译失败

// 例 2：Window 的 include 根是 Engine/Platform，不是 Engine
#include <Window/RenderWindow.hpp>                 // 实际：Engine/Platform/Window/
// 盲替换 → <Atom/Window/...>（不存在）；正确 → <Atom/Platform/Window/...>

// 例 3：Render 的 include 根是 Engine/Presentation
#include <Render/Renderer2D/Renderer2D.hpp>        // 实际：Engine/Presentation/Render/
// 盲替换 → <Atom/Render/...>（不存在）；正确 → <Atom/Presentation/Render/...>
```

原因是：**include 字符串只有和 target 的 include 根拼起来才有意义**。同一个
`<Utilities/...>`、`<Render/...>` 在不同 target 下可能指向不同目录；而迁移后所有 target 的
include 根统一为 `Engine`，所以旧字符串在新布局下不再成立。这就是为什么这一步是
**代码级迁移**，必须逐个文件按真实位置解析，不能写成一行 `sed`。

正确做法会产出一份「旧 → 新」的迁移报告（每个 include 一行），迁移后人工抽查 + 编译验证；
报告本身也可以作为 PR 附件，方便 review 这次机械改动。

#### 5.5.2 映射表与解析算法

| 旧 include 写法（示例） | 旧 include 根 | 迁移后 |
|---|---|---|
| `<Algorithm/...>`、`<Log/...>`、`<Error/...>`、`<Time/...>`、`<Concurrency/...>`、`<Threading/...>` | `Engine/Core` | `<Atom/Core/...>` |
| `<Utilities/Json/...>`、`<Utilities/Random/...>`、`<Utilities/Utf8/...>` | `Engine/Core` | `<Atom/Core/Utilities/...>` |
| `<Filesystem/...>`、`<Asset/...>`、`<Media/...>`、`<Utilities/Packager/...>` | `Engine/Resources` | `<Atom/Resources/...>` |
| `<ECS/...>`、`<Physics/...>`、`<Scene/...>`、`<World2D/...>` | `Engine/World` | `<Atom/World/...>` |
| `<Render/...>`、`<Layout/...>`、`<UI/...>`、`<Sprite/...>` | `Engine/Presentation` | `<Atom/Presentation/...>` |
| `<Window/...>`、`<Display/...>` | `Engine/Platform` | `<Atom/Platform/...>` |
| `<Platform/...>`（Paths/Dialogs/Entry）、`<Input/...>`、`<Backend/...>`、`<Runtime/...>`、`<Debugger/...>`、`<Lua/...>` | `Engine` | `<Atom/...>` |

> **注意 `Utilities/` 有歧义**：Core 和 Resources 下都有 `Utilities/`。所以**不要做盲目前缀替换**，
> 要用下面的解析算法。

#### 5.5.3 解析算法（推荐用 Python 脚本）

1. 扫描 `Engine/`、`Example/`、`Tools/`、`Test/` 下所有 `.hpp/.cpp`（排除 `ThirdParty/`）。
2. 对每个 `#include <X>`（跳过 `<Atom/` 开头、标准库、第三方）：依次在旧 include 根
   `Engine`、`Engine/Core`、`Engine/Resources`、`Engine/World`、`Engine/Presentation`、
   `Engine/Platform` 下查找 `X` 是否存在。
3. 恰好命中一个根 → 计算相对 `Engine/` 的路径 `R`，改写为 `#include <Atom/R>`。
4. 命中多个（如 `Utilities/...`）→ 按「包含它的文件所属模块」或完整相对路径判断；
   仍无法判断的，脚本输出清单，人工决定（预计只有个位数）。
5. 一个都不命中 → 视为第三方头，保持原样。

步骤：

1. 建目录并移动：`Engine/Core`、`Engine/Backend`、`Engine/Platform`、`Engine/World`、
   `Engine/Runtime`、`Engine/Presentation`、`Engine/Resources`、`Engine/Input`、
   `Engine/Lua`、`Engine/Debugger` → 各自 `Engine/Atom/<Name>`；`Engine/Atom.cpp` 已删除。
2. 按 5.5 的解析算法改写 include。脚本先判断是否已带 `<Atom/`，避免二次加前缀；
   `ThirdParty/` 与构建目录不改；歧义项（`Utilities/`）按算法第 4 步处理。
3. CMake：
   - 所有 `Engine/<Module>/...` 源路径 → `Engine/Atom/<Module>/...`；
   - 所有 `target_include_directories` 统一为 `${PROJECT_SOURCE_DIR}/Engine`；
     删除 `Engine/Platform`、`Engine/Core`、`Engine/Resources`、`Engine/World`、
     `Engine/Presentation` 这些特例；`Atom_Packager` 的第二个 include 目录一并去掉
     （其头文件改用 `<Atom/Resources/Utilities/Packager/...>` 包含）；
   - 根 `CMakeLists.txt` 的 `include(Engine/Presentation/Render/Shader/AtomShaders.cmake)`
     → `include(Engine/Atom/Presentation/...)`；`Engine/Targets.cmake` 不存在的路径同步；
   - `cmake/ApplicationEntry.cmake` 里的 include 目录保持 `Engine`。
4. 构建 + 全量测试，确认零行为变化；再用 `rg` 断言没有旧前缀残留。

> 这一步是纯机械迁移，必须单独一个 commit；不要和入口/导出宏混在一起，否则出问题无法二分定位。

### 5.6 公开头自包含检查

新增一个测试目标：一个 `.cpp` 只包含某个公开头，编译通过即证明该头不依赖隐藏的包含顺序。对全部安装头循环生成（可用 CMake 脚本或 Python 脚本）。这是防止「安装后缺头文件」的最有效手段。

### 5.7 静态检查（CI）

```powershell
# 公开头里不允许可变的 inline 变量
rg -n --pcre2 '^\s*inline\s+(?!constexpr|const\b)' Engine/Atom -g '*.hpp'
# 公开头里不允许函数局部 static（人工复核；也可用 clang-tidy）
rg -n '^\s+static\s+' Engine/Atom -g '*.hpp'
```

---

## 6. 分阶段实施计划

> 每个 Phase 一个独立 commit/PR；每步验收通过才进入下一步。

### Phase 0：基线（已完成）

- 输出目录、`Atom.cpp` 删除、`ApplicationConfig::fps`、81 测试全绿。
- 验收：`cmake --build` + `ctest`。

### Phase 1：include 前缀迁移

**目标**：源码树 `Engine/Atom/...`，所有引擎头 `<Atom/...>`，行为不变。

**改动**：见 5.5；同步更新 `CODING_STANDARD.md`、`API-Guide-CN.md` 中的 include 示例。

**验收**：

```powershell
cmake --build cmake-build-debug --target all --parallel 12
ctest --test-dir cmake-build-debug --output-on-failure   # 81/81
```

**回滚**：`git revert` 该 commit。

### Phase 2：入口

**目标**：`AtomMain.hpp` + `AtomEntry`/`AtomEntryWindowed`；16 个 example（15 个 `Example_*` + Homeless）改为实现 `ApplicationMain` 且只包含 `AtomMain.hpp`。

**改动**：

- 新增 `AtomMain.hpp`、`ConsoleMain.cpp`、`WindowsMain.cpp`；
- CMake：`Atom_Entry` / `Atom_Entry_Windowed` 两个 STATIC 库；`atom_application()` helper 替换旧 `atom_use_application_entry`；
- 删除 `Platform/ApplicationEntry.hpp/.cpp`（或保留一个版本的转发）；
- 示例逐个替换 include 与函数签名。

**验收**：构建 + 81 测试；手动启动 `AtomExample/Example_Simple_Window.exe` 和 `Example_ECS_Tour.exe`（控制台）。

**回滚**：`git revert`。

### Phase 3：导出宏骨架

**目标**：所有公开符号带 `ATOM_<MODULE>_API`；仍为 STATIC 构建，宏为空，行为不变。

**改动**：每模块 `Export.hpp`；类/自由函数/单例标注；CMake 定义 `ATOM_<MODULE>_EXPORTS`（仅 SHARED 时生效）。

**验收**：构建 + 81 测试；`rg 'ATOM_.*_API'` 覆盖公开头；无编译错误。

**注意**：这是机械但量大的改动，单独 commit，避免和其他变更混在一起。

### Phase 4：单 DLL 共享化 + install + find_package

**目标**：先用**一个** `Atom.dll` 跑通共享构建、安装、`find_package`、入口，验证 ABI 与单例。

**改动**：

- 新增 `ATOM_BUILD_SHARED` 选项；保留内部编译单元，将对象文件聚合到唯一的共享目标；
- `AtomEntry` 入口库；
- install/export 规则 + `AtomConfig.cmake.in` + `AtomHelpers.cmake`；
- 新增 `Test/SdkSmoke`（或 `Example/SdkSmoke`）：最小 `find_package` 工程。

**验收**：

```powershell
cmake -S . -B build-shared -G Ninja -DCMAKE_BUILD_TYPE=Release -DATOM_BUILD_SHARED=ON
cmake --build build-shared --parallel
ctest --test-dir build-shared --output-on-failure
cmake --install build-shared --prefix _sdk
cmake -S Test/SdkSmoke -B _smoke -G Ninja -DCMAKE_PREFIX_PATH=_sdk
cmake --build _smoke
./_smoke/HelloAtom.exe
```

**回滚**：`ATOM_BUILD_SHARED` 默认关闭，主构建不受影响。

### Phase 5：SDL3 共享 + Lua/ImGui API 导出

**目标**：`bin/` 出现 `SDL3.dll`；单共享目标导出 Lua C API 和 ImGui API。S6 再迁移到各自宿主。

**改动**：

- `SDL_SHARED ON`、`SDL_STATIC OFF`；安装 `SDL3.dll` 到 `bin/`；
- 单共享目标私有链接 SDL3 导入库，S6 拆分后由 Backends、Platform、Debugger 链接；
- Lua 源码编进本阶段共享库，构建时定义 `LUA_BUILD_AS_DLL`，
  消费方由 `Atom::Scripting` INTERFACE 定义 `LUA_BUILD_AS_DLL`（见 4.3.1）；
- ImGui 源码编进本阶段共享库，构建时 `IMGUI_API=__declspec(dllexport)`，
  消费方 `IMGUI_API=__declspec(dllimport)`；
- 安装 `include/lua/` 与 `include/imgui/` 第三方头；
- SdkSmoke 增加一个「自定义面板」变体，验证用户能 `#include <imgui.h>` 并链接 `Atom::Debugger`。

**验收**：

```powershell
dumpbin /dependents _sdk/bin/Atom.dll            # 确认链接 SDL3.dll
dumpbin /exports _sdk/bin/Atom.dll | findstr lua_
dumpbin /exports _sdk/bin/Atom.dll | findstr ImGui
# SdkSmoke 验证基础入口、自定义 main、ImGui 面板和 Lua 绑定
```

### Phase 6：拆分为 10 个 DLL

**目标**：按 4.3 拆分；用户 `find_package` component 与 DLL 一一对应；Debugger 始终在包里。

**重要**：这一步**一次性完成**，不要逐个模块切。原因：如果只把 Core 变成 DLL、其余仍是 STATIC，`Atom.World` 会被同时链进多个 DLL，导致状态重复。正确做法是同一变更里定义全部 10 个 DLL target，之后每个静态库只属于一个 DLL。

**顺序（构建依赖）**：Core → World/Media/Backends → Platform → Graphics/Runtime → Scripting → Debugger。

**验收**：

- 81 测试；
- SdkSmoke 分别只链 `Atom::Core`、`Atom::Runtime`、`Atom::Graphics`、`Atom::Debugger` 各跑一次；
- 每个 DLL 的依赖列表符合 4.3；
- `bin/` 只有预期的 DLL；
- `dumpbin /exports` 能看到 `lua_*` 与 `ImGui::` 符号。

### Phase 7：版本、CI、打包、发布

**改动**：

- `project(Atom VERSION 0.1.0)`；`Atom/Version.hpp`；`Initialize` 日志打印版本；
- 目标为 `master` 的 PR 在 Windows x64（MinGW GCC，MSYS2 UCRT64）和 Linux x64（GCC 14，Ubuntu 24.04）上构建并测试静态和共享 Release 版本。
- 正式发布时，为 `master` 上选定的提交推送 `v<version>` 标签。标签运行构建、测试并打包，每个工具链生成一个 ZIP，包含独立的 `static/`、`shared/` 安装前缀。
- 两项作业都通过后，标签运行发布两个 ZIP 和 SHA256 清单。

**验收**：将各平台的 ZIP 解压到干净目录，分别用 `static/` 和 `shared/` 前缀编译并运行 SdkSmoke。

---

## 7. 测试策略

| 层级 | 内容 | 何时跑 |
|---|---|---|
| 单元测试 | 81 个 CTest（`AtomTest/`） | 每次构建/PR |
| 入口测试 | 控制台 example、GUI example 启动即退出 | CI（GUI 可用 `SDL_VIDEODRIVER=dummy` 或 smoke 退出） |
| 安装后冒烟 | `Test/SdkSmoke`：只 `find_package(Atom)`，编译一个最小 `ApplicationMain` 并运行 | 每次 install 后 |
| 自定义面板冒烟 | `Test/SdkSmoke` 中的 `AtomSdkPanel`：通过 `Atom::Debugger` 在消费方创建 ImGui 上下文，验证面板位置和绘制数据 | S5 起，每次安装后 |
| 脚本绑定冒烟 | `Test/SdkSmoke` 中的 `AtomSdkLua`：通过 `Atom::Scripting` 注册消费方的 Lua 回调，执行脚本并读取结果 | S5 起，每次安装后 |
| 符号/依赖检查 | `dumpbin /dependents` / `objdump -p`；确认无重复 SDL、无 `lua.dll`/`imgui.dll`，`lua_*`/`ImGui` 从宿主 DLL 导出 | 发布前 |
| 公开头自包含 | 每个安装头单独编译 | PR |
| 工具链矩阵 | Windows MinGW GCC（MSYS2 UCRT64）和 Linux GCC 14 | 发布前 |
| 静态包 | `Atom.lib` 链接期符号完整；SdkSmoke 静态变体 | 发布前 |

`SdkSmoke` 的内容（约 30 行）：

```cpp
#include <Atom/AtomMain.hpp>
#include <Atom/Core/Log/LogSystem.hpp>
auto atom::platform::ApplicationMain(int, char**) -> int {
    atom::Log::SetConsoleOutputUtf8();
    LOG_INFO(atom::log::core::Main, std::string{"Atom SDK "} + atom::Version());
    return 0;
}
```

它验证：头文件能包含、导入库能链接、`AtomEntry` 的 `main` 能转发、DLL 能在运行时被找到、版本号能打印。

> S4 阶段还没有 `Version.hpp`，先打印字面量；S7 加版本头后改成 `atom::Version()`。

---

## 8. 风险登记表

| 风险 | 影响 | 规避 |
|---|---|---|
| 导出遗漏 | 用户链接失败 | Phase 3 全量标注 + SdkSmoke + 每模块 `dumpbin /exports` 检查 |
| 第三方被重复静态链接 | 两份 SDL/ImGui 状态，诡异崩溃 | SDL3 共享；Lua/ImGui 只编进各自宿主 DLL；其余第三方「一个宿主」规则 |
| 静态/共享混用 | 两份引擎状态 | 分成两个压缩包；文档明确「不要混链」 |
| 静态包与系统/第三方库重复 | 链接期重复符号 | 静态包的 `Atom.lib` 已含第三方；用户不再链 SDL3.lib/lua.lib |
| 静态初始化顺序跨 DLL | 注册表为空/初始化两次 | 后端/通道注册改为 `Initialize()` 显式调用 |
| Windows DLL 卸载时等待工作线程 | 程序从 `main` 返回后卡死 | 在 DLL 卸载前关闭日志和作业线程。验收同时覆盖入口库和自定义 `main` |
| ABI 混用 | 崩溃 | 每个包自包含；文档写明同工具链；`AtomConfig` 里做编译器/配置检查 |
| 循环依赖 | 无法构建 | 4.3 的 DAG；CI 用 `cmake --graphviz` 或脚本检查 |
| 公开头泄漏 SDL/内部实现 | 用户被迫装 SDL | 4.4 排除清单 + 自包含检查；ImGui/Lua 头是受控安装 |
| 用户又链一份 ImGui/Lua | 两份 context/状态 | 文档写明「不要另链」；导出符号检查；可选运行时断言 `ImGui::GetCurrentContext()` 非空 |
| DLL 找不到 | 程序启动失败 | `bin/` 与 exe 同目录；发布检查表验证 |
| 调试符号 | 崩溃栈不可读 | 每个包附 PDB（可选）；文档说明 |
| 包体积 | 下载大 | 每模块 DLL 只含自身；zip 压缩；不打包 PDB 到主包 |

---

## 9. 发布检查表

- [ ] `dev` 分支 CI 全绿（构建 + 81 测试）
- [ ] 共享包：`ATOM_BUILD_SHARED=ON` 构建全绿、install、SdkSmoke 通过
- [ ] 静态包：默认构建、install、合并 `Atom.lib`、SdkSmoke 通过
- [ ] 自定义面板冒烟（`Atom::Debugger` + `<imgui.h>`）通过
- [ ] Lua 绑定冒烟（`Atom::Scripting` + `lua_*`）通过
- [ ] `bin/` 依赖检查：无重复 SDL3，无 `lua.dll`/`imgui.dll`，无缺失 DLL
- [ ] `dumpbin /exports`：`lua_*` 在 Scripting、`ImGui` 在 Debugger
- [ ] `include/` 中没有被排除的 internal 头（脚本断言）
- [ ] 每个工具链的 shared/static 包都通过上述检查
- [ ] `atom::Version()` 输出与包名一致
- [ ] 版本号、许可证、README 已更新
- [ ] 工作流已创建对应提交的 `sdk-<commit SHA>` tag
- [ ] GitHub Release 附件与 SHA256 已上传
- [ ] 人工端到端验收（发布负责人用自己的小游戏完成，不属于本流程）

---

## 10. 问答归档（全部讨论）

### 10.1 第一轮：入口头与 FPS

| # | 问题 | 结论 |
|---|---|---|
| 1 | 用 `<Atom.hpp>` 伞形头代替 `<Platform/ApplicationEntry.hpp>`？ | 不做全能力头。入口单独 `AtomMain.hpp`；公共接口通过 `<Atom/Module/...>` 前缀暴露。 |
| 2 | `ApplicationConfig` 加默认 FPS，避免 `SetFPS(0)`？ | 已实现：`ApplicationConfig::fps`（0 = 不限帧），`Initialize` 应用一次；示例改为 config 设置。 |

### 10.2 第二轮：SDK 化后的入口

| # | 问题 | 结论 |
|---|---|---|
| 3 | `Atom.cpp` 只有打印日志，对 SDK 没用 | 已删除。入口由引擎提供 `main`，用户实现 `ApplicationMain`。 |
| 4 | main 在 exe 是否引擎侧决定 | 是。`AtomEntry` 提供 `main`/`WinMain`；用户不写 `main`。 |
| 5 | 单 DLL 还是多 DLL；第三方重复；体积/性能 | 选择拆分；SDL3 共享；其余第三方静态进唯一宿主；单 DLL 体积约 10–18 MB（Release，实测静态库合计 21.7 MB），拆分不影响运行性能。 |
| 6 | 拆成 Core/Media.Audio/Graphics 会不会出问题 | 可以；关键是每模块导出宏、第三方单一宿主、DAG 依赖、SDK3 共享。 |
| 7 | 单例/头文件状态 | 全仓审计：无违规；两处无害项（日志通道注册、Lua 兜底常量）。 |
| 8 | `find_package` 是否加文档即可 | 还需要生成 `AtomConfig.cmake`/`AtomTargets.cmake`；component 模型照 SFML。 |
| 9 | 安装后冒烟是发布后的测试 | 是发布前对「即将发布的包」的测试；自动化建议保留，人工验收由负责人完成。 |

### 10.3 第三轮：最终确认

| # | 问题 | 回答/结论 |
|---|---|---|
| 1 | DLL 划分 | 按 4.3 的 10 个；Layout 先并进 Graphics；Scripting 独立。 |
| 2 | Contracts 放哪 | `Atom.Core.dll`。 |
| 3 | 编译器 | MSVC / GCC / Clang / MinGW，通过 GitHub Actions 出包。 |
| 4 | 静态 SDK 与共享 SDK 区别 | 见 2.2；两种都要发，分成两个独立包。 |
| 5 | 第三方最终形态 | SDL3 共享；Lua 编进 `Atom.Scripting.dll` 并导出 C API；ImGui 编进 `Atom.Debugger.dll` 并导出 API（D03a/D18）。 |
| 6 | 版本与 tag | `0.1.0`，`v0.1.0-prebeta`。 |
| 7 | 交付形态/文档位置 | shared/static 各一个压缩包；`Docs/Plans/Specs/SDK-Packaging-CN.md`。 |
| 8 | CI 流程 | 先写入手册，评审后再决定保留项；入口保留 `AtomMain.hpp`；不做手动小游戏步骤（负责人流程外自测）。 |
| 9 | 删除 Atom.cpp 后是否需要 AllModules | 不需要。 |

### 10.4 第四轮：静态包、Lua、ImGui、Debugger

| # | 问题 | 结论 |
|---|---|---|
| 1 | 共享和静态 SDK 都能导出吗 | 能。两种都发，但**分成两个独立压缩包**，不要混在一个包里。 |
| 2 | Lua 必须单独构建 `lua.dll` 吗？改名 `Atom.Lua.dll`？ | **选择方案 2**：Lua 源码编进 `Atom.Scripting.dll`，用 `LUA_BUILD_AS_DLL` 导出 C API，用户链 `Atom::Scripting` 就能写绑定。不单独出文件；若以后想要独立文件，`Atom.Lua.dll` 只是改名。 |
| 3 | 「include 迁移不能盲加前缀」是什么意思 | 同一个文本 include 可能对应不同目录（`<Utilities/Json/...>` 在 Core，`<Utilities/Packager/...>` 在 Resources），且各 target 的 include 根不同；脚本必须按文件实际位置解析，不能统一加 `Atom/`。 |
| 4 | Debugger 只公开内置面板不好，用户要能写自定义面板，ImGui 符号冲突怎么解决 | 把 ImGui 只编进 `Atom.Debugger.dll`，并从该 DLL 导出 ImGui API（`IMGUI_API` 构建时 `dllexport`、消费方 `dllimport`）；随包提供 `imgui.h`。这样全进程一个 context，用户可写面板。 |
| 5 | Debugger 是否进 v1 包 | 必须进。 |

---

## 11. 统一口径的正确做法流程（最终执行版）

> 这是确认后唯一的执行顺序。每一步都必须「构建 + 测试 + 验收」通过后再进入下一步；任何一步失败，用 `git revert` 回到上一步。

### 11.0 总原则

1. **先机械、后功能、再打包**：include 前缀（纯机械）→ 入口（小功能）→ 导出宏（机械）→ 单 DLL（验证）→ 拆分（架构）→ CI/发布。
2. **每步可回滚**：一个 Phase 一个 commit；不把两个 Phase 混在一个提交里。
3. **静态构建必须始终可用**：`ATOM_BUILD_SHARED=OFF` 是引擎自身开发/测试的默认路径，任何阶段都不能破坏它。
4. **先让一个最小用户程序跑通，再谈拆分**：SdkSmoke 是每个 Phase 的守门员。
5. **同工具链、同配置**：每个包自包含，禁止混用。共享 MinGW 构建统一使用动态 C++/GCC 运行时，并打包实际依赖的运行时 DLL。MSVC/clang-cl 共享构建使用匹配配置的动态 CRT，Debug 包供安装了对应工具链的开发环境使用。

### 11.1 步骤总览

| 步骤 | 产物 | 验收 |
|---|---|---|
| S0 冻结基线 | `dev` 当前 3 个 commit | 81/81 |
| S1 include 前缀 | `Engine/Atom/**` + `<Atom/...>` | 构建 + 81/81 |
| S2 入口 | `AtomMain.hpp` + `AtomEntry` | 构建 + 81/81 + 手动启动两个 example |
| S3 导出宏 | 每模块 `Export.hpp` | 构建 + 81/81 |
| S4 单 DLL | `Atom.dll` + install + `find_package` + SdkSmoke | SdkSmoke 运行 |
| S5 SDL3 共享 + Lua/ImGui 导出 | `SDL3.dll`；`lua_*`/`ImGui` 从宿主 DLL 导出 | 依赖/导出检查 + 面板/Lua 冒烟 |
| S6 拆分 10 DLL | 10 个 DLL + components | 81/81 + 4 个 SdkSmoke 组合 |
| S7 CI/版本/发布 | shared + static 各一套包 + tag | 干净机器验证 |

### 11.2 S0：冻结基线

```powershell
git switch dev
git status                 # 必须干净
cmake --build cmake-build-debug --target all --parallel 12
ctest --test-dir cmake-build-debug --output-on-failure   # 81/81
```

若基线不绿，先修基线，不做任何 SDK 改动。

### 11.3 S1：include 前缀迁移（一个 commit）

1. 新建分支：`git switch -c sdk/include-prefix`。
2. 移动目录：

   ```powershell
   New-Item -ItemType Directory Engine/Atom
   foreach ($dir in @('Core','Backend','Platform','World','Runtime','Presentation','Resources','Input','Lua','Debugger')) {
       git mv "Engine/$dir" "Engine/Atom/$dir"
   }
   ```

3. 全局改写 include（`Engine/`、`Example/`、`Tools/`、`Test/`，排除 `ThirdParty/` 与构建目录）。
   注意：**不是简单加 `Atom/` 前缀**，要按 5.5 的「旧根 → 新路径」表映射，因为不同模块
   目前的 include 根不同。例如：
   - `<Algorithm/Vector/Vec2.hpp>`（Core 根）→ `<Atom/Core/Algorithm/Vector/Vec2.hpp>`
   - `<Filesystem/Vfs.hpp>`（Resources 根）→ `<Atom/Resources/Filesystem/Vfs.hpp>`
   - `<ECS/EntityWorld.hpp>`（World 根）→ `<Atom/World/ECS/EntityWorld.hpp>`
   - `<Render/Renderer2D/Renderer2D.hpp>`（Presentation 根）→ `<Atom/Presentation/Render/Renderer2D/Renderer2D.hpp>`
   - `<Window/RenderWindow.hpp>`（Platform 根）→ `<Atom/Platform/Window/RenderWindow.hpp>`
   - `<Runtime/Application.hpp>`（Engine 根）→ `<Atom/Runtime/Application.hpp>`

   脚本要先判断是否已带 `<Atom/`，避免二次加前缀；`#include "Application.hpp"` 这类
   同目录相对包含不改。
4. CMake：
   - 所有 `Engine/<Module>/...` 源路径 → `Engine/Atom/<Module>/...`；
   - 所有 `target_include_directories` 统一为 `${PROJECT_SOURCE_DIR}/Engine`（删除
     `Engine/Platform`、`Engine/Core`、`Engine/Resources`、`Engine/World`、
     `Engine/Presentation` 这些特例）；`Atom_Packager` 的第二个 include 目录去掉，
     头文件用完整路径包含；
   - 根 `CMakeLists.txt` 的 shader include 路径、`Engine/Targets.cmake` 的 include 路径同步。
5. 文档：`CODING_STANDARD.md`、`API-Guide-CN.md`、`README` 中的 include 示例同步。
6. 构建 + 81 测试；断言无旧前缀残留：

   ```powershell
   rg '#include <(Algorithm|Log|Error|Time|Concurrency|Threading)/' Engine Example Tools Test
   rg '#include <Utilities/' Engine Example Tools Test
   rg '#include <(Filesystem|Asset|Media)/' Engine Example Tools Test
   rg '#include <(ECS|Physics|Scene|World2D)/' Engine Example Tools Test
   rg '#include <(Render|Layout|UI|Sprite)/' Engine Example Tools Test
   rg '#include <(Window|Display)/' Engine Example Tools Test
   ```

   以上都应为空（`<lua.h>` 等第三方头除外）。
7. 提交：`refactor(sdk): move engine headers under Atom/ and prefix includes`。

### 11.4 S2：入口（一个 commit）

1. 新增 `Engine/Atom/AtomMain.hpp`（只有声明）。
2. 新增 `Engine/Atom/Platform/Entry/ConsoleMain.cpp`、`WindowsMain.cpp`。
3. CMake：`Atom_Entry`（STATIC）、`Atom_Entry_Windowed`（STATIC）；`atom_application(target WINDOWED|CONSOLE)` helper；替换旧的 `atom_use_application_entry`。
4. 16 个 example（15 个 `Example_*` + Homeless）：把 `#include <Atom/Platform/ApplicationEntry.hpp>` 改为 `#include <Atom/AtomMain.hpp>`；函数签名改为 `auto atom::platform::ApplicationMain(int argc, char** argv) -> int`。
5. 删除 `Engine/Atom/Platform/ApplicationEntry.hpp/.cpp` 与旧 CMake 逻辑。
6. 验收：构建 + 81 测试；启动 `AtomExample/Example_Simple_Window.exe`（GUI）与 `AtomExample/Example_ECS_Tour.exe`（控制台入口）。
7. 提交：`feat(sdk): add AtomMain/AtomEntry and migrate examples`。

### 11.5 S3：导出宏骨架（一个 commit）

1. 每个模块新增 `Export.hpp`（替换 `CORE` 为模块名）。
2. 给公开类/函数/单例加 `ATOM_<MODULE>_API`；先从 Core/Platform/Runtime/Graphics/Media/World/Scripting/Debugger 开始。
3. CMake：`target_compile_definitions(<target> PRIVATE ATOM_<MODULE>_EXPORTS)`；`ATOM_BUILD_SHARED` 作为 INTERFACE 定义。
4. 静态构建下宏为空，行为不变。
5. 验收：构建 + 81 测试；`rg -c 'ATOM_.*_API'` 覆盖所有公开头。
6. 提交：`build(sdk): add per-module export macros`。

### 11.6 S4：单 DLL 共享化 + 安装 + find_package + SdkSmoke（一个 commit）

1. 新增选项 `ATOM_BUILD_SHARED`（默认 OFF）。
2. 聚合引擎对象文件生成唯一的共享目标。静态默认路径保持独立。通过已安装的组件 target 暴露消费方所需的依赖。
3. 入口库保持 STATIC。
4. 新增 install/export（见 5.4）与 `AtomConfig.cmake.in`、`AtomHelpers.cmake`。S4 安装包提供不带版本号的 `find_package(Atom CONFIG)`，版本文件在 S7 加入。
   共享包同时安装预编译 shader，配置通过 `Atom_SHADER_DIR` 提供路径。
5. 新增 `Test/SdkSmoke/`（最小 `find_package` 工程）。
   Windows MinGW 的独立 DLL 试验已复现：日志调用成功且 `main` 返回，进程仍未退出。S4 必须让日志线程在 DLL 卸载前关闭并刷新队列，并检查同样会等待线程的作业系统单例。Windows 的 CMake 消费方在 EXE 侧自动注册清理，覆盖 `AtomEntry` 和自定义 `main`。冒烟测试同时验证日志刷新和退出码，并设置退出超时。
6. 验收：

   ```powershell
   cmake -S . -B build-shared -G Ninja -DCMAKE_BUILD_TYPE=Release -DATOM_BUILD_SHARED=ON
   cmake --build build-shared --parallel
   ctest --test-dir build-shared --output-on-failure
   cmake --install build-shared --prefix _sdk
   cmake -S Test/SdkSmoke -B _smoke -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=_sdk
   cmake --build _smoke
   ctest --test-dir _smoke --output-on-failure
   ```

7. 提交：`feat(sdk): shared build, install/export, and SDK smoke test`。

### 11.7 S5：SDL3 共享 + Lua/ImGui API 导出（一个 commit）

1. `SDL_SHARED ON`、`SDL_STATIC OFF`；不构建 `lua.dll`/`imgui.dll`。
2. 单共享目标私有链接 `SDL3.dll` 导入库。S6 拆分后由 Backends、Platform、Debugger 链接。
3. Lua 源码编进本阶段共享库：构建定义 `LUA_BUILD_AS_DLL`，
   INTERFACE 定义 `LUA_BUILD_AS_DLL`。
4. ImGui 源码编进本阶段共享库：构建 `IMGUI_API=__declspec(dllexport)`，
   INTERFACE `IMGUI_API=__declspec(dllimport)`。
5. Windows 安装 `SDL3.dll` 到 `bin/`。Lua 和 ImGui 公开头随对应组件提供。
   Linux 把 SDL3 共享库与 Atom 放在同一个库目录，由相对运行时搜索路径定位。
6. 验收：

   ```powershell
   dumpbin /dependents _sdk/bin/Atom.dll               # 确认链接 SDL3.dll
   dumpbin /exports _sdk/bin/Atom.dll | findstr lua_
   dumpbin /exports _sdk/bin/Atom.dll | findstr ImGui
   # 基础 SdkSmoke + 自定义面板 SdkSmoke + Lua 绑定 SdkSmoke
   ```

   MinGW 使用 `objdump -p` 检查导入和导出。共享构建的 `SdlError` 直接链接同一个 SDL3 DLL，
   验证测试端设置的错误能由 Atom 后端读取。安装包在搬迁后的前缀中运行四个 SdkSmoke。

7. 提交：`build(sdk): share SDL3 and export Lua/ImGui APIs from their host DLLs`。

### 11.8 S6：拆分为 10 个 DLL（一个 commit）

1. 按 4.3 定义 10 个 SHARED target；每个模块的静态库只属于一个 DLL。
2. 设置 `EXPORT_NAME`：`Core`、`World`、`Backends`、`Platform`、`Runtime`、`Graphics`、`Media`、`Audio`、`Image`、`Scripting`、`Debugger`、`Entry`、`EntryWindowed`。
3. DLL 的实际导入关系按 4.3。CMake 目标传递公开 API 所需的 Atom 模块，SDL3 保持为 DLL 的私有依赖。依赖图保持无环。
4. 共享 SDK 始终启用并打包 `Atom.Debugger`。静态开发构建继续使用原有调试开关。
5. 验收：

   - 静态构建 + 81 测试；
   - `ATOM_BUILD_SHARED=ON` + 81 测试；
   - 五个 SdkSmoke 分别使用 `Atom::Core`、`Atom::Runtime`、`Atom::Graphics`、`Atom::Debugger` 和 `Atom::Scripting`。额外检查 Core 入口程序和 Core DLL 的导入表，确认这条路径只加载 Core 与系统及编译器运行时。
   - `dumpbin /dependents` 每个 DLL 与 4.3 一致；
   - `bin/` 只含预期 DLL。

6. 提交：`feat(sdk): split runtime into per-module DLLs`。

### 11.9 S7：版本、CI、共享/静态打包、发布（一个 commit + tag）

1. `project(Atom VERSION 0.1.0)`；新增 `Atom/Version.hpp`：

   ```cpp
   #define ATOM_VERSION_MAJOR 0
   #define ATOM_VERSION_MINOR 1
   #define ATOM_VERSION_PATCH 0
   #define ATOM_VERSION_STRING "0.1.0-prebeta"
   namespace atom { auto Version() -> std::string_view; }
   ```

   `Application::Initialize` 启动日志打印版本。

2. GitHub Actions 工作流使用 `sdk-release.yml`：
   - 目标为 `master` 的 PR 在 Windows MinGW GCC（MSYS2 UCRT64）和 Linux GCC 14 上各编译并测试静态与共享的 Release 版本。
   - 推送 `v<version>` 标签时，两个工具链各自编译、测试、安装两种 SDK，并打包到各自的 ZIP。静态版在安装前构建 `AtomSdkArchive`。
   - 标签运行中的两个作业都通过后，发布 GitHub Release，附两个 ZIP 和 SHA256 清单。
3. 验收：将各平台 ZIP 解压到干净目录，用该包的工具链针对 `static/` 和 `shared/` 分别编译并运行 SdkSmoke。
4. 发布负责人为 `master` 上待发布的提交推送 `v<version>` 标签，触发发布流程。
5. 发布负责人用自己的小游戏做最终人工验收（流程外）。

### 11.10 提交与回滚策略

- 分支：每个 Phase 一个 `sdk/<phase>` 分支 + PR；合并到 `dev`。
- 提交信息：`<type>(sdk): <scope>`，正文写「改了什么/验收命令/回滚方法」。
- 回滚：`git revert <commit>`；S4 之后可先关 `ATOM_BUILD_SHARED` 回到静态路径。
- 禁止：在一个 commit 里同时改 include 前缀 + 入口 + 导出宏。

### 11.11 完成定义（DoD）

- [ ] 静态构建 81/81；
- [ ] 共享构建 81/81；
- [ ] 每个工具链的 shared/static 包 SdkSmoke 在干净前缀通过；
- [ ] 公开头只允许受控第三方（lua/imgui），SDL 不出现在公开头；
- [ ] `lua_*` 从 Scripting、`ImGui` 从 Debugger 导出；
- [ ] 10 个 DLL 的依赖与 4.3 一致，`bin/` 无多余文件；
- [ ] 静态包 `Atom.lib` 合并正确、无重复符号；
- [ ] `atom::Version()` 与包名一致；
- [ ] 文档（本篇 + API-Guide + README）同步；
- [ ] tag 与 GitHub Release 完成。

---

## 附录 A：DLL ↔ 现有 target 对照（实施时用）

| DLL | 现有 target |
|---|---|
| Core | Atom_Error, Atom_Log, Atom_Math, Atom_Algorithm, Atom_Color, Atom_Random, Atom_UTF8, Atom_Time, Atom_Threading, Atom_Concurrency, Atom_FS, Atom_Assets, Atom_Json, Atom_Packager, Atom_ECS, Atom_Sprite, Atom_Input（header-only 值类型）, Atom_InputSystem, Atom_Backend_Contracts |
| World | Atom_Physics, Atom_PhysicsIntegration, Atom_Scene |
| Backends | Atom_Backend_Extension, Atom_Backend_Composition, Atom_Backend_Runtime, Atom_Backend_Render_Composition, Atom_Backend_Render_Runtime, Atom_Backend_SDL3_Core, Atom_Backend_SDL3_Window, Atom_Backend_SDL3_Audio, Atom_Backend_SDL3_Mixer_Audio, Atom_Backend_SDL_GPU, Atom_Backend_Audio, Atom_Audio_Codecs |
| Platform | Atom_Window, Atom_Display, Atom_ApplicationPaths, Atom_FileDialogs |
| Runtime | Atom_Runtime |
| Graphics | Atom_Render_Core, Atom_Render_Pipeline, Atom_Renderer2D, Atom_Render_Resources, Atom_Text, Atom_UI, Atom_Layout, Atom_Scene2D, Atom_World2D |
| Media.Audio | Atom_Media |
| Media.Image | Atom_Image |
| Scripting | Atom_Lua（Lua 源码编入本 DLL 并导出 C API） |
| Debugger | Atom_Debugger, Atom_Backend_SDLGPU_Debug（ImGui 源码编入本 DLL 并导出 API） |

> 说明：
> - `Atom_ECS`、`Atom_Sprite` 放进 Core，是为了让 `Atom_Scene`（World）与 `Atom_Scene2D`
>   （Graphics）都只依赖 Core，避免 World ↔ Graphics 的环。
> - `Atom_World2D` 依赖 `Atom_Scene2D`，暂放 Graphics；以后要独立分发再拆
>   `Atom.World2D.dll`，依赖图不变。
> - `Atom_Audio_Codecs` 被 `Atom_Backend_Composition` 使用，放 Backends，避免
>   Backends → Media 的反向依赖。
> - `Atom_InputSystem` 只依赖 `Atom_Backend_Contracts` 与 `Atom_Log`，放 Core；
>   Window 在 Platform，依赖 Core 即可。

## 附录 B：第四轮确认结果（无待定项）

| 问题 | 最终决定 |
|---|---|
| 共享/静态 SDK | 两种都发，分成 `...-shared.zip` 与 `...-static.zip`；静态包是单个合并后的 `Atom.lib`。 |
| Lua 是否要 `lua.dll` | 不要。Lua 编进 `Atom.Scripting.dll` 并导出 C API；用户链 `Atom::Scripting` 即可写绑定。若以后想要独立文件，改 `OUTPUT_NAME` 为 `Atom.Lua.dll` 即可，不影响其它设计。 |
| include 迁移 | 按 5.5 的「文件解析」算法迁移，不盲加前缀。 |
| Debugger 自定义面板 | 支持。ImGui 编进 `Atom.Debugger.dll` 并导出 API；随包提供 `imgui.h`。 |
| Debugger 是否进 v1 | 必须进。 |

确认后即可开始 S1。

## 附录 C：未来方向（明确不在 v1）

| 方向 | 说明 | 为什么不在 v1 |
|---|---|---|
| ImGui 封装成 Atom 模块 | 例如 `Atom.ImGui.dll` + Atom 风格 `Panel`/`Widget` 包装层，用户不直接接触原始 ImGui 头；升级 ImGui 由引擎负责 | 需要稳定的封装 API 与版本策略；v1 先保留原始 ImGui + 从 Debugger DLL 导出，给用户最大自由度 |
| Lua 独立成 `Atom.Lua.dll` | 把 Lua 打成独立模块，Scripting 依赖它 | 多一个文件与依赖；当前「编进 Scripting + 导出 C API」已满足绑定需求 |
| 静态包按模块拆分 | 现在静态包是一个合并的 `Atom.lib`；以后可拆成 `Atom.Core.lib` 等，支持更细的按需链接 | 合并库已经够用，链接器会丢弃未引用对象；拆分只影响包体积/可选性 |
| Linux / macOS 正式包 | v1 只发 Windows x64；Linux 在 CI 验证，macOS 未纳入 | 需要各平台的人工验收与运行时依赖梳理 |
| 插件 / 热加载 | 运行期加载 Atom 模块或用户插件 | 需要 ABI 版本协商与生命周期管理，远超「分发」范围 |
| C ABI facade | 让任意编译器都能链接 Atom | 需要重做公开接口，工作量与风险都很大；v1 明确同工具链 |

> 这份文档是 SDK 打包与发布的**唯一口径**。后续任何设计变更，先改这里，再改代码。

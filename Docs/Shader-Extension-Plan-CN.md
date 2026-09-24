# Shader 分层与外部编译机制设计方案

> 状态：设计文档（待实现）
> 关联：Render/Shader/AtomShaders.cmake / SDLGPU2DPipelineFactory / Renderer2D

## 〇、根本原则：职责边界（首要）

shader 分层与外部编译机制的**根本原因**是游戏侧与引擎侧的职责边界：

> **游戏侧（Example 及游戏开发者项目）绝对不能反向修改引擎侧（Atom）。**

- 引擎侧（`Render/Shader`）提供**通用**能力与**编译工具链函数**，是稳定、可复用的下层。
- 游戏侧只**消费**引擎能力，通过引擎公开的扩展点接入自己的 shader，**不碰**引擎源码
  与 `AtomShaders.cmake`。
- 任何需要游戏侧改动引擎侧才能生效的方案（如往 `AtomShaders.cmake` 里追加 shader、
  在 `SDLGPU2DPipelineFactory` 里硬编码外部 shader 名）都是**错误方向**，一律否决。

下述「可插拔蒙版」「外部编译机制」都服务于这条边界：让游戏侧有自己的 shader 归属地，
并有一条不侵入引擎的接入路径。

## 一、问题

1. 当前 `Render/Shader/Source` 里的三个后处理 shader
   （ChromaticAberration / Glitch / GaussianBlur）在 uniform 里硬编码了
   `uRegion`（归一化区域）+ `uMask`（corner radius / feather），并在 shader 内
   写了整段「圆角矩形 SDF 蒙版」。这是 MusicCard「卡片圆角弹窗」的场景绑定，
   不是纯通用算法。
2. 游戏开发者要引入自己的 shader，目前只能改 `AtomShaders.cmake`（在里面追加
   `atom_compile_shader(...)`），这**绝对不可接受**——引擎的构建脚本不应被使用方
   修改（违反职责边界）。

## 二、方案 B：把圆角蒙版做成「可插拔」

蒙版职责与后处理算法职责分离，各归其位：

### 2.1 分层原则

| 层 | 归属 | 内容 |
|----|------|------|
| 通用后处理算法 | `Render/Shader/Source` | 纯全屏 screen-space 滤镜（色差/毛刺/高斯模糊），**无任何蒙版概念** |
| MusicCard 专属组合 | `Example/MusicCard/shaders` | 「圆角卡片蒙版 + 后处理」的场景绑定 |

### 2.2 可插拔蒙版的具体形态

把圆角矩形 SDF 蒙版抽成一个**可复用的 GLSL 片段**（如 `mask_rounded_rect.glsl`，
通过 `#include` 或 GLSL 编译期拼接引入），后处理 shader 通过一个开关
（`uMaskEnabled` / `uMask.z > 0.5`）决定是否应用蒙版：

- 开关关闭 → 纯全屏算法，零蒙版开销（当前已通过 `has_region` 语义链路实现）。
- 开关打开 → 引用 mask 片段，计算逐像素软边缘 alpha。

这样蒙版逻辑只写一份，通用后处理不再内联 MusicCard 的 SDF。

> 注：圆角软边缘是逐像素 alpha，**scissor 矩形硬裁剪替代不了**（scissor 只能做
> 直角裁剪），所以圆角蒙版必须留在 shader 侧；矩形裁剪已由 C++ 侧
> `SDL_SetGPUScissor` 承担。

## 三、外部 shader 编译：不改 AtomShaders.cmake

### 3.1 现状的三个障碍

1. `atom_compile_shader(source)` 的 `source_path = ${ATOM_SHADER_SOURCE_DIR}/${source}`，
   而 `ATOM_SHADER_SOURCE_DIR` 写死为 `CMAKE_CURRENT_LIST_DIR/Source`，外部 shader
   不在这里就编不了。
2. 函数定义与内置调用混在同一个 `.cmake` 里，`include` 即触发全部内置编译。
3. 运行时加载（`SDLGPU2DPipelineFactory.cpp`）硬编码了内置 shader 名字
   （`LoadSDLGPUShader(..., "GaussianBlur.frag.glsl", ...)`），外部 shader 即使编译
   出来，也没有运行时加载入口。

### 3.2 目标设计

把 `atom_compile_shader` 改造成**接受绝对路径**（或显式 source dir）的通用函数，
并把「函数定义」与「内置调用」分离：

```cmake
# 函数签名（示意）：接受绝对 source 路径 + stage，产出 spv/dxil/msl
atom_compile_shader_abs(ABS_SOURCE_PATH STAGE)
# 或：保留 atom_compile_shader(source stage) 但内部走一个可配置的 source dir
```

游戏开发者在自己项目的 `CMakeLists.txt` 里：

```cmake
include(Render/Shader/AtomShaders.cmake)   # 拿到函数 + 工具链变量（glslc/dxc/...）
atom_compile_shader_abs("${CMAKE_CURRENT_SOURCE_DIR}/shaders/MyEffect.frag.glsl" frag)
# 再把产物加入自己的 custom target，并 add_dependencies 到自己的可执行文件
```

### 3.3 待解决的问题（需在实现前敲定）

1. **include 语义**：`.cmake` 里 `find_program(... REQUIRED)` 重复 include 会重跑
   find（无害但冗余），需要确认 `ATOM_SHADER_OUTPUT_DIR` 是否要参数化，避免外部
   项目把产物写进引擎的 binary 目录。
2. **运行时加载入口**：`SDLGPU2DPipelineFactory` 目前只加载硬编码的内置 shader。
   外部 shader 要真正用起来，需要一个「按名字/路径加载外部预编译 shader」的公开
   入口（例如 `IRender2DContext` 或 SDLGPU backend 暴露一个 `LoadExternalShader`）。
3. **uniform 布局契约**：外部后处理 shader 若要复用 PostProcess 的
   `set=3, binding=0` 布局，需明确约定 uniform 结构，否则各写各的。

## 四、推荐落地顺序

1. 先做「可插拔蒙版」：抽 `mask_rounded_rect` 片段，通用后处理改为开关引用；
   MusicCard 专属 shader（如需）移到 `Example/MusicCard/shaders`。
2. 再做「外部编译」：把 `atom_compile_shader` 参数化（绝对路径），分离定义与调用。
3. 最后补「外部运行时加载」入口。

## 五、待办

- [ ] 抽取可复用圆角 SDF 蒙版片段，通用后处理去内联化（方案 B）
- [ ] `atom_compile_shader` 支持绝对路径 / 外部 source dir
- [ ] 外部 shader 运行时加载入口设计
- [ ] 一个「游戏开发者引入自定义 shader」的 Example

# Atom 2D 游戏就绪度：任务清单与实施计划

> 更新日期：2026-10-29（§3.2 五处被低估的工作量；A0.1–A0.3、T1.1–T1.3、T2.1–T2.3 已实施，见各项状态）
> 目标：Windows/Linux 桌面端、键盘鼠标优先的通用 2D 引擎底座。它优先支持正交 Tilemap 与中大型可流送世界，
> 但不把经营、RPG、横版、弹幕等任何具体玩法写进引擎核心。
>
> 本文是“从现有引擎底座到第一个可玩 vertical slice”的实施清单。`Docs/Remaining-Issues.md` 仍是所有引擎
> 整改项的汇总入口；本文只展开游戏就绪度路线、顺序与验收。

## 1. 状态说明与成功标准

| 标记 | 含义 |
|---|---|
| `[x]` | 已完成并至少有构建或自动化测试验证 |
| `[-]` | 已有基础实现，仍缺接入、功能或验收 |
| `[ ]` | 已确定要做，尚未开始 |
| `[!]` | 有明确价值，但不得阻塞当前 vertical slice |

### P0 完成定义

P0 的完成不是“API 能编译”，而是一个新示例能在 Windows 和 Linux 上完成下面整条链路：

```text
启动 -> 加载一个 Scene -> 生成角色、障碍物和 HUD
     -> 键盘输入驱动角色 -> 固定步物理与碰撞/触发事件
     -> Sprite/Camera 绘制 -> 暂停 -> 重新开始 -> 退出
```

场景加载失败不能留下半个世界；游戏实体销毁不能在 ECS 遍历中发生；资源、物理对象与 GPU 资源在重新开始和退出后
都必须按所有权顺序释放。P0 不要求开放世界流送、动画、粒子、手柄、编辑器或联网。

## 2. 已有基线

| 领域 | 状态 | 已有能力 | 尚缺的游戏层 |
|---|---|---|---|
| Runtime | `[-]` | `Application` 是唯一主循环拥有者；Screen 生命周期、固定步、事件泵和 shutdown 顺序已存在 | `GameScreen` 对 `GameWorld` 的实际接入、加载/重开流程 |
| GameWorld | `[-]` | 固定的 Input → FixedUpdate → Update → LateUpdate；稳定注册顺序；命令缓冲；FIFO 事件队列 | 没有游戏组件、Scene、渲染/物理系统消费它 |
| ECS | `[-]` | 安全 Entity handle、显式组件存储、查询与迭代 | 只有基础 Position/Velocity 等组件；没有游戏系统 |
| 2D 渲染 | `[-]` | 纹理、source rect/atlas、批渲染、基础 Camera 数学、层、裁剪、文本 | SpriteRenderer、CameraComponent、排序与 RenderSystem |
| UI | `[x]` | Panel/Label/Button、输入命中、焦点、模态层、Runtime 示例 | 游戏实际 HUD、暂停和重开流程；TextField/手柄导航为后续 |
| 物理 | `[-]` | 2D 刚体、接触、关节、固定步、Internal backend | ECS 绑定、触发器、查询、单位转换、debug draw |
| 资源 | `[-]` | VFS、APKG、Texture/Script/Audio 资源管理 | Scene loader、依赖图、预算、热重载与 cook |
| 音频/脚本 | `[-]` | 音乐、SFX、流式播放、Lua Host | 游戏事件到音频的映射；实体/场景脚本层 |

已完成实现的入口：

- `Runtime/Application`：窗口、时间、Screen 和帧阶段组合。
- `Runtime/GameWorld`、`SystemScheduler`、`EntityCommandBuffer`、`GameEventQueue`：P0 世界运行基础。
- `UI/`：P0 菜单/HUD 控件。
- 详见 [2D-Core-Scope-v1-CN.md](2D-Core-Scope-v1-CN.md)。

## 3. 架构边界（冻结）

```text
Screen        菜单、暂停、加载、设置等“呈现导航”
GameWorld     一个运行中的 ECS world、系统、命令和事件
Scene         一个可序列化的世界片段；可以是小关卡或未来的 world chunk
Prefab        可复用的实体模板与有限字段覆盖
WorldSession  多个 Scene/WorldChunk 的加载、卸载和持久状态（后续）
```

`Screen` 绝不等于 `Scene`。首个 `GameScreen` 拥有一个 `GameWorld`，并在安全边界请求加载、卸载或替换 Scene。
`WorldSession` 与无缝流送不是 P0 的前置条件；P0 的一个 Scene 只需能成为它将来的一个 chunk。

### 固定帧顺序

```text
Application：事件/输入采样 -> 固定步时钟 -> Screen
GameWorld   ：Input -> commit -> FixedUpdate × N -> commit
                         -> Update -> commit -> LateUpdate -> commit
Render      ：读取已提交的插值/显示状态 -> 提交 draw packet
UI/Overlay  ：HUD、暂停和工具层 -> Present
```

系统注册顺序就是执行顺序。系统可直接修改**已有组件的值**，但实体创建、销毁、添加/删除组件等结构变更必须进入
`EntityCommandBuffer`，在阶段边界提交。事件使用 FIFO 队列，消费方必须明确其消费阶段；Runtime 不隐式调用游戏回调。

### 3.1 2026-10-24 架构复核：P0 前置收敛门

这次复核确认：大子系统本身已经具备可复用基础，但它们之间仍有几个**依赖方向和目录归属**问题。若直接实施
T1.1，会把资源、渲染和具体 backend 反向拖入 ECS，后续每增加一个“很小的”游戏能力都会扩大耦合。因此以下 A0
任务是 T1.1 的前置门，不是可选的文档美化。

| 规则 | 冻结决定 |
|---|---|
| `ECS` 组件 | 只持有值、稳定 id、字符串或纯算法类型；不得持有 `Renderer2D::Texture*`、`TextureHandle`、SDL/GPU/物理 backend 指针或 `AssetPath` 等会给 ECS 引入高层模块的类型。资源解析由 Render/Scene2D、Scene loader 等系统完成。 |
| 数学与游戏数据 | `Algorithm/` 仅保留无所有权、无资源访问、无平台依赖的数学值类型；`ecs::Transform2D` 可以使用独立的游戏数据结构，不能把 entity 生命周期塞进 `algo::Transform2D`。Sprite pivot 属于 Sprite 数据而不是通用 world transform。 |
| backend contract | `Backend/Contracts` 只能依赖基础值类型与必要的字节流/输入契约，绝不依赖具体 SDL、SDL_GPU、渲染器或完整 Physics 域实现。每个 public include 都必须由 CMake 的 `PUBLIC`/`INTERFACE` 依赖显式表达。 |
| Physics | 物理 interface、形状值类型、Internal backend 和 ECS integration 属于同一 Physics 域；Render 只读取同步后的显示 Transform，绝不拥有 body。 |
| 游戏 Runtime | `Runtime/` 是 Application、Screen、GameWorld、调度和生命周期；`Backend/Composition` 是后端装配层，不是游戏 Runtime，二者目录已经分离。 |
| 示例与工具 | `Example/` 只验证能力，不容纳引擎通用实现；`Utilities/Packager` 是工具，非运行时游戏模块。 |

### 3.2 2026-10-25 实施复核：五处被低估的工作量

对照 `dev` 分支实际代码逐项复核后确认：原有 A0/T1/T2/T3 的任务描述**不足以覆盖落地所需工作**。以下五项不是新增
功能，而是原任务里遗漏的实做内容与依赖；它们必须在对应任务内一并完成，否则该任务的验收无法达成。

| 编号 | 遗漏项 | 现状证据 | 归入 |
|---|---|---|---|
| G0.1 | `Screen` 与 `GameWorld` 的帧节奏不一致 | `Application.cpp:84-98` 每帧调用 `Screen::FixedUpdate` 0..N 次、`Update` 1 次；`GameWorld::RunFrame(delta, steps, fixed_dt)` 一次跑完四个阶段，`RunPhase` 仍是 private | T1.3 |
| G0.2 | 查询组合的显式实例化税 | 新增组件需改三处：`ECS/Components.hpp:84`、`ECS/EntityWorld.cpp:309-318`、每种查询组合一行的 `ATOM_ECS_INSTANTIATE_ITERATION`（`EntityWorld.cpp:341-346` 目前只有两个组合） | A0.1 |
| G0.3 | `Renderer2D` 能力缺口与两个孤儿值类型 | `DrawTexture` 只接受轴对齐 `algo::Rect`，无翻转/旋转/pivot/逐次采样器；`BeginFrame(origin_x, origin_y, zoom)` 无旋转、无 `Camera2D` 入口；`algo::Camera2D` 与 `algo::Transform2D` 除自身 `.cpp` 外零调用者 | T1.2 |
| G0.4 | 物理契约缺触发器、查询与 user-data | `IPhysicsWorld` 只有 Begin/End 接触回调；`BodyDef`（`Physics/Contracts/PhysicsTypes.hpp:31`）无 sensor 标记、无 user-data；无 raycast/形状查询 | T3.1 |
| G0.5 | 内容根与 CI 缺口 | 仓库无 `Assets/` 内容根，`MusicCardScreen.cpp:781` 在运行时把宿主目录挂成 `res://`；`.github/workflows/build.yml` 只 `cmake --build`，从不执行 ctest | T1.3 / T4.2 |

**落地状态（2026-10-25）**：G0.1（`Screen::BeginFrame` + `GameScreen` 批处理，测试 `Atom_Runtime.GameScreenFrame`）、
G0.2（规则写入 `ECS-Component-Vocabulary-CN.md` 与 `EntityWorld.cpp` 实例化区块）、G0.3 的翻转/相机映射/提交前排序、
G0.5 的 `Assets/` 内容根与 `res://` 开发挂载均已落地；G0.3 的最近邻采样器与旋转精灵仍未做（不阻塞 M2），
G0.4 与 G0.5 的 CI 部分随 T3.1 / T4.2 处理。

#### G0.1 帧节奏：`Screen` 与 `GameWorld` 的阶段所有权不一致

`GameWorld` 把四个阶段原子地跑完，而 `Screen` 只被告知"这一帧该跑几次固定步"的**结果**，拿不到步数。若不先决策，
T1.3 只能二选一：在 `FixedUpdate` 里每次跑一个 `RunFrame(0, 1, dt)`（Input/Update/LateUpdate 被放大 N 倍），或在
`Update` 里跑 `RunFrame(delta, N, fixed_dt)`（`Screen::FixedUpdate` 形同废弃，且固定步对 Screen 不可见）。两者都会
把错误语义固化进示例。T1.3 的第一步必须是显式决策，方向建议：让 `Screen` 拿到"本帧固定步数"，或让 `GameWorld`
暴露按阶段推进的入口，使"Input 每帧一次、FixedUpdate N 次、Update/LateUpdate 每帧一次"仍然成立，并且该顺序可被
headless 测试断言。

#### G0.2 查询实例化：显式列表的税

A0.1 原任务只回答"哪些组件可以进 `ATOM_ECS_COMPONENTS`"（依赖方向问题），没有回答"查询组合怎么增长"。当前每
出现一个新的 `With<A,B>()` / `ForEach<A,B>()` 调用点，都要回到 `ECS/EntityWorld.cpp` 增加一行实例化，这意味着
T1.1、T1.2、T3.1 会依次修改引擎文件。决策：**P0 接受逐组合显式实例化**（编译期可控、无运行时注册），但必须把
列表中文化集中在 `EntityWorld.cpp` 一处，并注释"新增查询 = 增加一行"；若组合数量超过可维护阈值，再单独评估类型
擦除式迭代，**不得**在 T1.1 期间偷渡动态组件注册。

#### G0.3 渲染器：能出图，但画不出"精灵"

`DrawTexture` 支持图集子矩形与色调，这是 Sprite 的最小可用面；但下列四项缺失，前两项决定 T1.2 的边界：

1. **翻转**：无 flag、无负数宽高处理。角色左右朝向是任何 2D 玩法的第一需求，必须补；最小改法是给 `DrawTexture`
   增加 `flip_x`/`flip_y`，在生成 UV 后交换 U 或 V。
2. **相机桥接**：`algo::Camera2D` 已有 `ViewMatrix`/`WorldToScreen`/`ScreenToWorld` 且是完整值类型，却无人使用；
   `Renderer2D` 只认 `BeginFrame(origin, zoom)`。T1.2 必须显式定义 `Camera2D` 的 position/viewport/zoom 如何映射到
   `BeginFrame`，并明确 **P0 不支持 `Camera2D::rotation`**（写进文档，不得静默忽略）。
3. **排序键**：`Renderer2D.cpp:415` 只按 layer 稳定排序，层内顺序等于提交顺序。因此 `(layer, sort_key, entity)`
   的排序必须在 Scene2D 提交前完成；这是 T1.2 的实现要求，不是渲染器的责任。
4. **采样器**：全局 Linear，无法表达最近邻。像素风项目需要它；P0 可延后，但必须写进 T1.2 的验收说明，避免美术
   接入后才发现。

旋转同理：`Transform2D::rotation` 在 P0 允许存在但**不可见**，T1.2 的验收不得声称"旋转精灵正确绘制"。

#### G0.4 物理契约：T3.1 不是纯集成层

原任务把 `Physics/Contracts` 视为已冻结。实际缺：`BodyDef` 无 sensor/trigger 标记（触发器会产生解算响应）；无
user-data（`IContactListener` 只交出 `IPhysicsBody*`，无法可靠映射回 entity，除非维护旁路表）；`IPhysicsWorld` 无
raycast/形状/点查询；接触生命周期只有 Begin/End，没有 persist。因此 T3.1 必须拆成两步：先扩契约与 Internal 后端
（sensor 不产生速度响应、user-data 或稳定 id、至少点/射线查询），再做 ECS 绑定；估计量按此上调。

#### G0.5 内容根与 CI

示例资源目前来自宿主目录（`MusicCardScreen.cpp:781` 运行时 `NativeFileSystem::Create("res", ...)`），仓库内没有
可交付的内容根。vertical slice 需要一条约定：`Assets/` 目录 + `res://` 挂载点 + CMake 复制规则（或运行期以源码
树为根的开发挂载），否则 `res://` URI 在示例中没有意义。CI 侧：`build.yml` 只构建不测试，T4.2 的第一步是把
`ctest` 加入两个平台的 job。

### 阶段 A0：架构收敛与目录校正（T1.1 前必须完成）

**硬门：** A0.1、A0.2、A0.3 与 A0.6 必须在 T1.1 前完成，因为它们决定组件、target 与错误依赖的方向。
A0.4/A0.5 必须在各自领域出现新功能前完成，可与硬门并行；不得为了赶进度把新的 Scene2D、codec 或 Video 代码
继续放入旧的错误边界。

- [x] **A0.1 — 组件可扩展性与 ECS 依赖边界。**
  - 问题：当前 `EntityWorld` 的显式实例化由 `ECS/Components.hpp` 单一列表驱动。若将含资源或渲染类型的
    `SpriteRenderer` 直接加进去，`Atom_ECS` 会反向依赖 Asset/Render，随后形成 ECS ↔ Render 的循环。
  - 决策：首批 Scene2D 组件在 ECS 中仅保存纯数据（例如 `texture_uri`/稳定资源 key、source rect 数值、颜色、
    layer、排序键），不保存资源 handle。T1.1 实施前要写出“哪些组件可进入当前显式列表”的准入规则；长期若需
    插件组件，再单独重构组件注册/实例化机制，不在 P0 偷渡动态注册。
  - 补充（G0.2 查询实例化）：A0.1 同时决定“查询组合怎么增长”。P0 接受逐组合显式实例化，但要求
    `ECS/EntityWorld.cpp` 的实例化列表集中、带注释，并把“新增查询 = 增加一行”写成规则；禁止在 T1.1 期间引入
    动态组件注册或类型擦除式迭代。验收附带一条：新增 `With<A,B>()`/`ForEach<A,B>()` 组合只改动该文件的一处
    列表，不得分散到多个 `.cpp`。
  - **已落地（2026-10-25）**：准入规则与实例化策略冻结在
    [ECS-Component-Vocabulary-CN.md](ECS-Component-Vocabulary-CN.md)；规则摘要写入 `ECS/Components.hpp` 头注释，
    实例化规则写入 `ECS/EntityWorld.cpp` 的迭代区块。CMake 修正了隐藏依赖：`Components.hpp` 公开 include
    `Algorithm/Vector/Vec2.hpp`，但此前只把 `Atom_Algorithm` 以 PRIVATE 链接；现改为 `PUBLIC Atom_Math`
    （`Vec2` 的新属主），并移除对 `Atom_Algorithm` 的边缘依赖。`ECS/` 公开头文件不含任何 `Render/`、`Asset/`、
    `Physics/`、`Media/`、`Runtime/`、`Backend/` include。
  - 验收：`Atom_ECS` 的 public include 和 CMake 依赖仍只到 Algorithm、Error、Log 等基础层；Scene2D target 可以
    依赖 ECS，反方向不成立。

- [x] **A0.2 — 消除 Contracts 的隐藏反向依赖。**
  - 已审计问题：`IRenderBackend.hpp` 在 public API 使用 `algo::Vec2`，但 `Atom_Algorithm` 又依赖
    `Atom_Backend_Contracts`；`IAudioDecoder.hpp` public include `Filesystem/FileSystem.hpp`，而 Contracts target
    没有显式声明 FS；`Physics/Contracts/PhysicsTypes.hpp` 又 include `Physics/Shapes/*`。这些在当前单仓库
    include 路径下能编译，却不能形成可移植、可导出的 target 图。
  - 工作：将 Render contract 改用 Contracts 自有的 extent/value type 或拆出真正无依赖的 value target；为 Audio 的
    `IFile` 契约建立正确的低层依赖；把 Physics contracts 与 Internal backend 收敛到 `Physics/Contracts` 与
    `Physics/Backend/Internal`，移除 `Backend/Contracts -> Physics` 的方向。
  - **已落地（2026-10-25）**：新增叶子 target `Atom_Math`（`Vec2/Vec3/Vec4/Mat3/Mat4/Rect/Primitives/Scalar/
    Easing`，文件仍留在 `Algorithm/`，只改 target 归属），`Atom_Algorithm` 收敛为 `Transform*`+`Camera*` 并
    PUBLIC 依赖 `Atom_Math`。`Atom_Backend_Contracts` 的公开依赖改为显式声明：
    `INTERFACE Atom_Math Atom_Color Atom_Input` + `INTERFACE Atom_FS`（在 `Atom_FS` 定义处追加，避免前向引用）。
    同时删除三条**空边**：`Atom_Algorithm -> Atom_Backend_Contracts`、`Atom_UTF8 -> Atom_Backend_Contracts`、
    `Atom_Log -> Atom_Backend_Contracts`（三者的头文件都不 include 任何 Backend 契约，正是这些空边造出了环）。
    现在 `Contracts -> {Math, Color, Input, FS}`、`FS -> Error`、`UTF8 -> UtfCpp`、`Algorithm -> Math`，无环。
  - 验收：每个 public header 的直接依赖都由 target 的 PUBLIC/INTERFACE 依赖表达；不存在 Algorithm ↔ Contracts、
    Filesystem/UTF8/Contracts 或 Physics ↔ Backend Contracts 的 target 环。

- [x] **A0.3 — Error 平台捕获下沉。**
  - 问题：`Atom_Error` 直接链接 SDL3，并公开 `CaptureSdlError()`；这违背 Error 作为最低层错误值的定位，且让
    Filesystem/Asset/ECS 等所有错误消费者间接依赖 SDL。
  - 工作：保留平台无关的 `Error`、errno/system-error 格式化；将 SDL 错误捕获移动至 `Backend/SDL3/Core`（或
    SDL 专属诊断帮助器），由失败点把已捕获文字写入 Error。
  - **已落地（2026-10-25）**：新增 `Backend/SDL3/Core/SdlError.{hpp,cpp}`（`CaptureError` 读并清除、
    `PeekError` 不清除、`FormatErrorSuffix` 产出 `"; sdl=..."`），并从 `Atom_Error` 删除 `CaptureSdlError` 与
    `<SDL3/SDL_error.h>`。`Atom_Error` 不再链接 `Atom_3rd_SDL3`；`ErrorTest` 移除 SDL 用例，新行为由
    `Atom_Backend_SDL3_Core.SdlError` 覆盖。`CaptureErrnoMessage`/`CaptureLastSystemError` 保留（平台无关的
    errno 与 Win32 格式化不引入 SDL）。
  - 验收：`Atom_Error` 不再链接/包含 SDL；纯 ECS、Filesystem、Asset 测试可在没有 SDL backend implementation 的
    情况下链接。

- [~] **A0.4 — 目录职责机械迁移。**
  - 已完成：`Backend/Runtime` → `Backend/Composition`；`Backend/Audio/Decoder` → `Media/Audio/Codecs`；
    `Backend/Physics/Internal` 与 `Backend/Contracts/Physics` → `Physics/Backend/Internal` 与 `Physics/Contracts`。include、
    CMake source list、测试与文档引用已同步更新。
  - 剩余：A0.2 仍需真正解除 contracts 的依赖环；本项只校正文件归属，不把“移动成功”误当作 target 边界已经正确。
  - 命名待办：`Physics/Backend/Internal` 的领域归属正确，但与根级 `Backend/` 容易混淆。下一次仅重命名的机械迁移应改为
    `Physics/Backends/Internal`；不得与 A0.2 的 contract 依赖拆分或任何新物理功能混在同一个变更中。
  - 验收：`Backend/` 只保留 backend implementation、registry/extension 和 composition；`Physics/`、`Media/Audio/`
    各自拥有其领域的 contract 与可替换实现。

- [~] **A0.5 — 空壳与双轨模块处理。**
  - 已完成：移除无实现且命名空间错误的 `Media/Video/Video.*`，并从 `Atom_Media` source list 删除；未来 Video 必须以
    明确的 Experimental 设计和真实最小能力重新进入仓库，不能复用该空壳。
  - `Render/Renderer2D` 仍含旧字体、glyph atlas 与 text layout 实现，而 `Render/Text/` 已建立新 contract。冻结规则：
    新 Scene2D/UI 工作只接 Text 模块接口；旧路径只允许做兼容和迁移，不能继续增加功能。
  - 验收：每个公开模块都至少有真实能力、测试和 README；不存在“同一职责两套都可继续扩展”的路径。

- [~] **A0.6 — 聚合 target 与构建拓扑。**
  - 问题：`Atom_Core` 聚合 Runtime、Lua、Packager、媒体、渲染、物理等几乎全部 target；它适合当前空入口，但不应
    成为游戏或工具的默认依赖，否则模块化只停留在目录层面。
  - 已完成：全量聚合 target 改名为 `Atom_All`；`Atom_Core` 保留为兼容 shim，新入口只链接 `Atom_All`。新代码禁止以
    `Atom_Core` 作为默认依赖。
  - 剩余：导出面向消费者的细粒度 `Atom::*` targets；新 vertical-slice example 显式链接其所需
    Runtime/ECS/Scene2D/Physics/UI/Assets target，不链接任何全量聚合。
  - 验收：CI 至少有一个最小 ECS/GameWorld target、一个 Runtime+UI target 和一个完整示例 target；它们都不靠
    “全量链接碰巧补齐头文件依赖”。

## 4. P0 任务清单：最小可玩闭环

### 阶段 0：边界与运行时基础

- [x] **T0.1 — Runtime 唯一帧循环。**
  - 落点：`Runtime/Application.*`、`Runtime/ScreenManager.*`。
  - 验收：Window 不再拥有第二套游戏循环；Screen/GPU 资源先于 backend 释放。

- [x] **T0.2 — GameWorld、固定系统阶段、命令缓冲与事件队列。**
  - 落点：`Runtime/GameWorld.*`、`Runtime/System.*`、`Runtime/SystemScheduler.*`、
    `Runtime/EntityCommandBuffer.*`、`Runtime/GameEventQueue.*`。
  - 已验证：`Atom_Runtime.GameWorld` 覆盖阶段/注册顺序、阶段边界创建销毁、事件 FIFO、非法时间参数和运行中禁止改调度表。
  - 尚缺：应用级 `GameScreen` 尚未拥有它；当前事件只有类型与 source/target，实际事件语义由后续 Physics/玩法模块定义。

- [x] **T0.3 — P0 游戏 UI。**
  - 落点：`UI/`、`Example/UI/UiRuntimeDemo.cpp`。
  - 验收：菜单、按钮、暂停覆盖层、键鼠焦点/模态输入可工作。

### 阶段 1：游戏对象与 2D 显示（A0 完成后）

- [x] **T1.1 — 冻结首批游戏组件词汇。**
  - 前置条件：A0.1、A0.2、A0.3、A0.6 完成；A0.4/A0.5 在涉及的目录新增功能前完成。
  - 新增位置：`ECS/Components.hpp` 只放纯数据；跨域系统实现不放进 ECS。
  - 首批组件：`Transform2D`（位置、旋转、缩放）、`RenderTransform2D`（插值后的只读显示状态）、
    `SpriteRenderer`（预验证的 texture URI/key、source rect 数值、pivot、layer、排序键）、`Camera2D`、
    `Lifetime`、`SceneEntityId`。Physics 的 body/collider 壳放在 `Physics/Integration/`。
  - 不做：Animator、Tilemap、任意脚本对象、游戏特有 Health/Inventory/Quest 组件。
  - **已落地（2026-10-25）**：六个组件按准入规则加入 `ECS/Components.hpp` 与实例化列表；`SpriteRenderer` 用
    `texture_uri` 字符串而非句柄，`Transform2D` 不带 pivot、`Camera2D` 不带矩阵（与 `algo::` 同名类型职责分离）。
    顺带补齐了 `ATOM_ECS_INSTANTIATE` 的四种 `Emplace` 形态（默认构造 / rvalue / 非 const 左值 / const 左值）
    ——原先只有 rvalue 一种，`Emplace<T>(e)` 与 `Emplace<T>(e, sprite)` 会在链接期失败，这次由测试暴露并修正。
    新增五个查询组合（`Transform2D`、`Transform2D+RenderTransform2D`、`RenderTransform2D+SpriteRenderer`、
    `Transform2D+Camera2D`、`Lifetime`、`SceneEntityId`），每个都有真实调用点与测试。
  - 验收：组件能安全注册、查询、在命令缓冲回调中创建；组件不会泄漏 SDL、GPU 或第三方物理类型。
    （`Atom_ECS.ComponentVocabulary` + `Atom_Runtime.CommandBufferScene`）
  - 预计：约 400–800 行实现与测试。

- [x] **T1.2 — Render/Scene2D 的 Sprite 和 Camera 系统。**
  - 新增位置：`Render/Scene2D/`；依赖 `ECS`、`Renderer2D`、资源句柄，不反向依赖 Runtime。
  - 工作：从 `Transform2D + SpriteRenderer` 收集 draw item；按 layer/sort/entity 稳定排序；应用 `Camera2D`；
    提交既有 Renderer2D。RenderSystem 只读显示 Transform，不写模拟坐标。
  - 首期范围：一张 texture、source rect、颜色、翻转、pivot、层排序；仅一个活动相机。
  - 补充（G0.3 渲染器前置）：T1.2 必须同步做三件渲染器侧工作——(1) `Renderer2D::DrawTexture` 增加
    `flip_x`/`flip_y`，否则角色朝向无法表达；(2) 显式定义 `algo::Camera2D` 的 position/viewport/zoom 到
    `BeginFrame(origin, zoom)` 的映射，并注明 P0 不支持 `Camera2D::rotation`；(3) 把 `(layer, sort_key, entity)`
    的稳定排序放在 Scene2D 提交前完成（`Renderer2D` 只按 layer 稳定排序，层内顺序等于提交顺序）。
    最近邻采样器与旋转精灵明确不在 T1.2 内，验收说明不得声称支持。
  - **已落地（2026-10-25）**：新增 `Atom_Scene2D` target（`Render/Scene2D/`），依赖 `Atom_ECS`、`Atom_Renderer2D`、
    `Atom_Render_Resources`，**不依赖 Runtime**。分工是 `SnapshotTransforms`（模拟→显示副本）、
    `FindActiveCamera`（`Transform2D`+`Camera2D`→`CameraView`，取最低实体索引，无相机时可退化）、`Collect`
    （只读 `RenderTransform2D`+`SpriteRenderer`，解析 URI、算 pivot/size/scale、排序、诊断计数）、`Submit`
    （唯一触碰 GPU 的一步，不做任何决策）。G0.3 三项前置全部完成：`DrawTexture` 新增 `flip_x`/`flip_y`（交换 UV，
    不改几何与绕序）；相机映射写成 `origin = center - viewport/(2*zoom)` 并注明 P0 不支持旋转；排序在 Collect 内按
    `(layer, sort_key, entity)` 完成。`ITextureResolver` 把 GPU 依赖抽象成一处，`TextureCacheResolver` 负责 URI→句柄
    的备忘与所有权。headless 测试 `Atom_Scene2D.SpriteRenderer` 覆盖排序稳定性、pivot/scale 目标矩形、图集子矩形、
    失效/缺失纹理诊断、相机取值与退化。
  - 验收：同一实体集合在稳定输入下产生稳定 draw 顺序；相机平移/缩放正确；不可见或失效纹理有明确 fallback/诊断。
  - 预计：约 1,200–2,000 行实现、测试与 headless/recording 验证；含上述渲染器侧改动约 +200–400 行。

- [x] **T1.3 — GameScreen 接入与无资源硬编码 smoke scene。**
  - 新增位置：`Example/Game/` 或独立 `Example_2D_Vertical_Slice`；不得污染 `Atom.cpp`。
  - 工作：`GameScreen` 创建/销毁 `GameWorld`，将 Application 的固定步和 variable update 转发给它；先用硬编码
    实体验证 T1.1/T1.2，随后由 T2 Scene 替换。
  - 补充（G0.1 帧节奏决策，必须最先做）：`Application` 每帧调 `Screen::FixedUpdate` 0..N 次、`Update` 1 次，
    而 `GameWorld::RunFrame` 原子跑完四个阶段且 `RunPhase` 为 private。T1.3 第一步必须显式决策二者映射，使
    “Input 每帧一次、FixedUpdate N 次、Update/LateUpdate 每帧一次”成立且可被 headless 测试断言；不得用
    “`FixedUpdate` 里跑完整 `RunFrame`”或“`Update` 里吞掉固定步”这类会放大阶段次数的临时接法。
  - 补充（G0.5 内容根）：示例需要一个仓库内的 `Assets/` 内容根与 `res://` 挂载约定（开发期以源码树为根或
    CMake 复制），否则 `SpriteRenderer` 的 texture URI 在示例中没有意义；挂载与卸载必须在帧安全点完成。
  - **已落地（2026-10-25）**：
    - G0.1 决策：`Screen` 新增 `BeginFrame(fixed_steps, fixed_delta_seconds)` 钩子（默认空实现，不破坏既有 Screen），
      `Application` 在派发固定步**之前**发布本帧预算，`ScreenManager` 只转发给当前 Screen。新的
      `Runtime/GameScreen`（引擎内、可复用）记录该预算，并在 `Update` 里调用一次
      `GameWorld::RunFrame(delta, N, dt)`；`FixedUpdate` 故意不覆写。理由与两种被拒方案都写在
      `GameScreen.hpp` 顶部。`Atom_Runtime.GameScreenFrame` 用阶段记录器断言 `input → fixed×N → update → late`
      的顺序、N=0 时仍跑 Input/Update/LateUpdate、预算不被跨帧重放、暂停不推进世界、setup 失败即构造失败。
    - G0.5 内容根：内容根归属**示例自己**（`Example/Game/Assets/`，`textures/*.png` 由同目录 `generate_textures.py` 可复现生成），不做仓库根共享根；开发期挂载约定：
      `Example/Game/SmokeMain.cpp` 在启动时把 `ATOM_SOURCE_ROOT/Example/Game/Assets` 以 priority 200 挂到 `res://`；发布期同一批
      URI 改由 APKG 提供，上层不变。
    - 示例：`Example/Game/` 的 `SmokeScreen`（`GameScreen` 子类）+ `SeedSmokeScene`，用真实 `res://` 贴图搭出
      40×25 地砖、两个箱子、一个图集子矩形、一个玩家；`move-player`（FixedUpdate）与 `camera-follow`（LateUpdate）
      两个系统按阶段注册；目标 `Example_2D_Smoke` 已构建并实机运行。
  - 验收：无 Scene loader 时也能创建一个角色和静态障碍、按键移动、由 SpriteRenderer 绘制。
    （`Atom_Runtime.GameScreenFrame` + `Example_2D_Smoke` 实机运行，见 `Example/Game/smoke-scene.png`）
  - 依赖：T1.1、T1.2。
  - 预计：约 500–1,000 行示例与集成测试。

- [x] **T1.4 — 固定步插值与显示缓冲（本轮新增）。**
  - 动机：逻辑固定 60 步/秒，显示可能是 144Hz 或忽快忽慢。直接画最新状态会"一顿一顿"，这与渲染性能无关，
    是采样率不匹配。
  - 落点：`Screen::BeginFrame` 增发 `interpolation_alpha`（来自 `DomainClock::Alpha()`）；`GameScreen` 保存并钳制到
    [0, 0.999]；`Render/Scene2D` 的 `Collect(world, alpha)` 与 `FindActiveCamera(..., alpha)` 把
    `RenderTransform2D`（上一步）与 `Transform2D`（这一步）混合。
  - 关键决定：`SnapshotTransforms` 从"每帧一次"改为"每个固定步一次、且是 FixedUpdate 的第一个系统"，否则
    `RenderTransform2D` 就只是最新状态的拷贝，插值无从谈起；混合结果只进 draw item，**不写回组件**（写回会毁掉
    上一步状态，表现为逐渐滞后）；相机用同一个 alpha，避免"精灵平滑、相机跳步"的整屏抖动。
  - 顺带清理：`GameScreen::OnWorldFrameAdvanced`（T1.3 的临时钩子）删除——快照现在是系统，注册顺序就是它的语义。
  - 验收：alpha=0 画上一步、0.5 画中点、1 被钳制到 0.999 不外推；`Collect` 不修改任一组件；
    `Atom_Scene2D.SpriteRenderer`（第 8、9 节）+ `Atom_Runtime.GameScreenFrame`（alpha 钳制与暂停时仍发布）。

### 阶段 2：Scene 与 Prefab（先可加载，再通用）

- [x] **T2.1 — Scene schema 与资源格式决策。**
  - 新增位置：`Scene/` 或 `Runtime/Scene/`；格式文档放 `Docs/Plans/`。
  - 首期建议：人可读 JSON/JSONC 源文件，显式 `schema_version`，所有资产使用 `res://` URI；发布期可再 cook 为二进制。
  - 最小字段：Scene id/version、实体局部 id、名称、Transform、Sprite、Camera、后续 Physics 壳、资源 URI。
  - 明确拒绝：未知组件、未知字段、循环 Prefab、跨 Scene entity 引用、任意 C++/Lua 对象序列化。
  - **已落地（2026-10-25）**：格式与校验规则冻结在 [Scene-Schema-v1-CN.md](Scene-Schema-v1-CN.md)（JSONC 语法、单位与
    坐标约定、字段表与默认值、未知字段一律拒绝、错误诊断格式、与 ECS 组件的唯一映射、v2 预留清单）。样例与反例在
    `Example/Game/Assets/scenes/`：`smoke_room.scene.json`（6 个实体，覆盖全部 v1 字段）+ `Test/Data/scenes/` 的三个反例（`invalid_unknown_field,
    invalid_duplicate_id,invalid_bad_version`），已用脚本验证语法可解析。
  - **剩余（一项决策）**：解析器选型。文档 §8 给出 A（自研 `Utilities/Json` tokenizer，推荐）与 B（引入
    `nlohmann/json`）的对比与理由；确认后 T2.2 按所选方案实现。该决策只影响实现方式，不影响已冻结的格式。
  - 验收：schema 有样例、版本号和错误诊断规范；选择/引入的解析器不泄漏进 ECS/Render 公共 API。
  - 依赖：T1.1。

- [x] **T2.2 — SceneData、SceneLoader 与原子切换。**
  - 新增位置：`Scene/`（`SceneTypes`、`SceneSchema`、`SceneLoader`）+ `Utilities/Json/JsonReader`。
  - 工作：VFS 读取；解析/语义校验；在临时世界里建实体；所有步骤成功后才替换 active world。
    加载失败保留旧 world 并返回携带 URI、实体 id 和字段名的 `Error`。
  - **已落地（2026-10-25）**：新增 `Atom_Scene` target（依赖 ECS/FS/Log/Math/Color，**不依赖 Runtime**）。
    `SceneLoader::Load` 读 VFS → `ParseScene` → `BuildWorld`，返回一个**全新的** `EntityWorld`；切换由
    `GameWorld::ReplaceEntities` 的一次指针交换完成，系统与帧计数保留，待提交命令与事件丢弃（它们引用旧实体）。
    活动相机先创建，从而拿到最低实体索引，`Render/Scene2D` 因此不需要"active"标志。
    `Example/Game` 已改为加载 `res://scenes/smoke_room.scene.json`：房间内容全部来自文档，只有地板仍是代码生成的
    装饰（T5.1 交给 Tilemap）；系统改为按 `SceneEntityId` 查实体，不再缓存句柄。
  - **顺带发现的坑（已写进代码与文档）**：`Entity` 是 (index, generation)，而 generation 属于**某个世界的槽表**。
    换世界后新世界第一个实体的 (index, generation) 与旧世界的第一个实体相同，`IsAlive()` 因此会误判旧句柄仍有效。
    新增 `GameWorld::GetWorldGeneration()`：系统把 generation 与句柄一起缓存，变化即按 `SceneEntityId` 重查。
  - 验收：有效 Scene 加载后实体数量/组件/引用正确；缺资源、无效 URI、重复 id、版本不支持和未知组件都不会部分替换
    当前世界。（`Atom_Scene.JsonReader`、`Atom_Scene.SceneLoader`、`Atom_Runtime.WorldReplace`）
  - 依赖：T1.1、T1.2、T2.1。
  - 预计：约 1,500–2,500 行实现与测试。

- [x] **T2.3 — 平面 Prefab。**
  - 工作：一个 Prefab 是受同一 schema 约束的实体模板；Scene 内可实例化并覆盖白名单字段。
  - 首期不做：Prefab 嵌套、循环检测图、instance 热重载 merge、编辑器 round-trip。
  - **已落地（2026-10-29）**：prefab 是独立文件（`{schema_version, kind:"prefab", id, entity}`），实例写作
    `{id, prefab, overrides?}`。`ParseScene` 新增 `PrefabProvider` 回调，prefab 在**校验阶段**就解析进来，所以覆盖项
    是对着 base body 校验的（例：prefab 没有 `sprite` 时 `overrides.sprite` 直接报错）。
    覆盖白名单：`name`/`tags`（整体替换）/`lifetime`/`transform.*`/`sprite.{tint,layer,sort_key,flip_x,flip_y}`/
    `camera.{zoom,viewport,active}`；`sprite.texture|source|size|pivot` 一律拒绝——实例可以决定实体在哪、什么颜色、
    哪一层、活多久，但不能改变它**是什么**。加载器按 URI 在一次加载内缓存 prefab；prefab 加载失败 = 场景加载失败。
    v1 禁止嵌套，因此不可能出现循环。示例的两个木箱已改成同一 prefab 的两个实例。
  - 验收：同一 Prefab 多实例的 override 互不污染；无法解析的 Prefab 使整个 Scene 加载失败而不破坏旧 world。
    （`Atom_Scene.ScenePrefab`，以及 `Atom_Scene.SceneLoader` 对**随仓库发布的** prefab 文件的断言）
  - 依赖：T2.2。
  - 预计：约 700–1,200 行实现与测试。

- [x] **T2.4 — 图集元数据（美术管线第一层，本轮新增）。**
  - 动机：没有这一层，每个精灵的矩形/尺寸/轴心都要写进每个实体；美术给一张 8×8 图集就得手动数像素。它也是
    "像素风必须最近邻采样"唯一能落地的地方。
  - 新增位置：`Sprite/`（`Atom_Sprite`：`SpriteAtlas` + `SpriteAtlasLoader`）；格式文档
    [Sprite-Atlas-v1-CN.md](Sprite-Atlas-v1-CN.md)。
  - 工作：一张纹理 + 命名帧（`rect` 必填，`size`/`pivot` 可选并有默认值）+ 整表一个 `filter`；场景 `sprite` 块新增
    **互斥**的 `atlas`+`frame` 写法，把帧解析成 `SpriteDesc`（ECS 组件不变，只多一个 `SpriteFilter` 值字段，映射点
    只有两处：Scene loader 与 Scene2D）。
  - 顺带补完 G0.3 的一半：`Renderer2D::DrawTexture` 新增 `render::Filter2D` 参数并真正创建 Nearest sampler，因此
    `filter: "nearest"` 不是元数据摆设——它改变屏幕像素。旋转精灵仍缺。
  - 不做：帧序列/状态机（T5.3）、九宫格（UI 未来需求）、图集打包工具、纹理合并。
  - 验收：帧矩形/尺寸/轴心/过滤正确落到组件；两种 sprite 写法互斥；缺帧列出可用帧名；随仓库发布的图集有测试。
    （`Atom_Sprite.Atlas`、`Atom_Scene.SceneAtlas`、`Atom_Scene2D.SpriteRenderer`）

### 阶段 3：物理、输入与可玩验证

- [~] **T3.1 — Physics/ECS 集成层。**（拆成 T3.1a 契约 + T3.1b 集成两轮）
  - 新增位置：`Physics/Integration/`，绝不放入 `ECS/` 或 `Render/`。
  - 前置（G0.4 契约扩展，必须先做）：现有 `Physics/Contracts` 不足以支撑本任务。需先补
    `BodyDef::is_sensor`（触发器不产生速度/位置响应）、body 的稳定 user-data 或 id（否则 `IContactListener`
    拿到的 `IPhysicsBody*` 无法可靠映射回 entity，只能靠旁路表）、至少点/射线查询，以及接触 persist 语义
    （目前只有 `BeginContact`/`EndContact`）。这些是契约与 Internal 后端的改动，不是集成层可以兜住的。
  - **T3.1a 已落地（2026-10-29）**：`BodyDef::is_sensor` + `BodyDef::user_data`（uint64，创建后不可变——契约里
    不放 `void*`，否则等于邀请"拥有者已销毁仍被读"）；`IPhysicsBody::IsSensor()/GetUserData()`；
    `IContactListener::PersistContact()`（顺序固定 Begin → Persist → End）。Internal 后端里传感器**只**把接触对
    放进当前集合（因此事件照常上报），**不**喂求解器、**不**唤醒任何人——一个分支就是"触发器 vs 碰撞体"的全部差别。
    射线/点查询**按先前决定留到有真实需求时再做**，不进这一轮。
  - 工作（T3.1b）：`PhysicsBodyComponent`/`ColliderComponent` 只保存 engine 侧 id 与配置；创建/销毁在命令边界同步；
    FixedUpdate 执行物理；模拟结果写 `Transform2D`；Render 读取插值结果。
  - 单位：物理固定使用 MKS、y-up、弧度；game/render 边界集中做 pixels-per-meter 与 y 轴转换，禁止散落换算。
  - 事件：将碰撞开始/持续/结束和 trigger 事件转为 `GameEvent`，顺序稳定、无回调重入。注意
    `GameEventQueue::GameEvent` 目前只有 `{type, source, target}`，无 payload；需要 collider 细节时须先决策
    是扩事件结构还是让消费方回查组件。
  - 验收：销毁实体不会遗留 body；触发器不产生物理解算响应；固定帧率变化下结果一致；debug draw 可核对 collider。
    （T3.1a：`Atom_Physics.SensorAndUserData` 断言穿行不留响应、persist 计数、真实碰撞体仍会挡住、user_data 可反查）
  - 依赖：T0.2、T1.1、T2.2。
  - 预计：约 1,200–2,000 行实现、测试与可视化示例；含契约扩展约 +400–800 行。

- [x] **T3.2 — 游戏输入动作边界。**
  - 工作：示例玩法只消费 `MoveLeft`/`MoveRight`/`MoveUp`/`MoveDown`/`Interact`/`Pause` 等游戏 action，
    不直接散落读取具体 Key。现有 `ActionMap` 保持键盘优先，但 API 需预留多个 binding source。
  - 不做：手柄映射、键位设置 UI、云同步。
  - **已落地（2026-10-29）**：
    - 引擎侧新增 `Input/ActionState.hpp`（`Atom_InputSystem` 内，header-only）：把 action 变成
      `Axis()` / `Movement()`（已归一化，对角线不会更快）/ `IsPressed|IsHeld|IsReleased`，并提供**唯一一道玩法闸门**
      `SetGameplayEnabled()`。闸门关掉时所有玩法查询读中性值，且**不动绑定**——恢复时不需要重绑。
    - 动作枚举属于游戏：`Example/Game/GameInput`（`GameAction` + 默认绑定 + `SetBindings` 整体换 profile）。
      演示玩法系统不再出现任何 `Key` 比较；暂停键走**事件路径**（`ActionMap::FindAction(event)`），因此不受玩法闸门
      影响——否则暂停会变成单向门。`Application::DispatchEvent` 先喂 `InputSystem` 再给 Screen，所以 Screen 消费
      事件不会吞掉输入，这条顺序已核对。
    - "预留多个 binding source"的落点：所有查询都经过同三个函数，第二个来源（手柄）＝多一个 map、在这三个函数里
      多做一次查找，调用点不变；profile 可整体替换（`SetBindings`）就是键位重映射要用的接缝。
  - 验收的验证：`Atom_Input.ActionState` 断言"换绑定不改调用点"（同一查询在重绑后返回相反结果）、闸门关闭时
    `Movement` 为 0 而 `FindAction` 仍能取到 Pause、对角线归一化、pressed 只存活一帧。
  - 依赖：T1.3。

- [ ] **T3.3 — 第一个 vertical slice。**
  - 新增位置：`Example/2DVerticalSlice/`，使用真实 Scene、至少一个 Prefab、Sprite、物理障碍和 UI。
  - 内容：角色、静态障碍、一个 trigger/终点、HUD、暂停、重开；美术可以是占位资源。
  - 验收：本节“P0 完成定义”全部满足；Windows/Linux CI 均构建该 example，并运行无窗口逻辑测试。
  - 依赖：T1.3、T2.3、T3.1、T3.2。

### 阶段 4：P0 质量门禁

- [ ] **T4.1 — 无窗口/Recording render 测试后端。**
  - 工作：断言 RenderSystem 的排序、相机变换、frame/pass 顺序和资源生命周期，不依赖人工截图。
  - 依赖：T1.2。

- [ ] **T4.2 — 跨平台 CI 测试矩阵。**
  - 工作：Windows 与 Linux 都执行 ECS/Scene/GameWorld/Physics 核心测试；新增 Scene 测试资源使用 VFS，禁止绝对路径。
  - 现状（G0.5）：`.github/workflows/build.yml` 两个平台都只做 `cmake --build`，**从不执行 ctest**，所以本任务是
    “让现有 60+ 个 CTest 真正进 CI”，而不是从零搭矩阵；第一步即为两个 job 各加一次 `ctest --output-on-failure`。
  - 验收：任何一个平台的 schema、物理同步或 deterministic 排序回归都能阻断合并。

- [ ] **T4.3 — P0 内存与生命周期回归。**
  - 工作：反复 load/restart/unload scene，覆盖 texture handle、physics body、Screen、GameWorld 和事件队列。
  - 验收：ASan/UBSan 可用平台纳入 CI；无悬垂句柄、未释放 body 或使用已销毁 GPU 资源。

## 5. P1 任务清单：2D 内容生产与大世界准备

- [ ] **T5.1 — 正交 Tilemap 与对象层。**
  - Tile layer、object layer、tileset 元数据、chunk 边界、图块碰撞生成；对象层保留自由放置实体。
  - Tilemap 不得成为 SpriteRenderer 的特殊分支，应产出可批处理的 draw item 和 collider 描述。
  - 依赖：T1.2、T2.2、T3.1。

- [ ] **T5.2 — Camera 行为层。**
  - 跟随、边界、平滑、视差、屏幕震动和 Scene 转场；`Camera2D` 数据组件保持与数学 Camera 分离。

- [ ] **T5.3 — 逐帧动画。**
  - SpriteSheet/Atlas 元数据、clip、Animator 状态机、帧事件、翻转、pivot。
  - 动画帧事件进入 `GameEventQueue`，不能直接重入改 ECS 结构。

- [ ] **T5.4 — WorldManifest、WorldSession 与同步切块。**
  - `WorldManifest` 声明 Scene/Chunk 坐标、边界和入口；`WorldSession` 管理当前 active chunk 集合。
  - 首版同步加载/卸载相邻 chunk，先解决跨 chunk 坐标、实体归属、传送门和持久 id，再考虑后台流送。
  - 这项是“星露谷式世界”的首个引擎级能力，不等于现在实现无限地图。
  - 依赖：T2.2、T5.1。

- [ ] **T5.5 — 存档、设置与音频事件。**
  - 版本化 save data；用户配置与键位；Master/Music/SFX/UI bus、音效池、场景切歌。
  - 存档记录游戏定义的持久数据与 Scene/Prefab stable id，不序列化任意内存对象或第三方指针。

- [ ] **T5.6 — 开发诊断。**
  - 实体检查、碰撞/触发器可视化、资源统计、帧时间、系统阶段耗时和地图/chunk overlay。

## 6. P2 与后续：规模、表现和可选类型能力

- [ ] **T6.1 — 异步资源加载与 WorldChunk 流送。**
  - 资源依赖图、取消、安全替换、内存预算、预加载半径和加载失败降级；必须在 T5.4 的同步语义稳定后实施。

- [ ] **T6.2 — 粒子系统。**
  - 发射器、生命周期曲线、对象池、批渲染、预热、预算与调试可视化。
  - 粒子不是“一颗一个 ECS entity”；只把 emitter 配置挂在实体，模拟粒子使用专用连续存储。

- [ ] **T6.3 — 输入扩展与可访问性。**
  - SDL 手柄 binding、键位重映射 UI、导航焦点、字体 fallback、本地化、缩放与减少动态效果选项。
  - 手柄是后续能力：P0 不直接读取 SDL joystick，也不把 keyboard chord 写死到玩法系统。

- [ ] **T6.4 — 类型扩展包。**
  - 横版：one-way platform、slope、coyote time、jump buffer。
  - 俯视角世界：交互、昼夜、背包、任务、NPC 日程、寻路。
  - 动作/弹幕：命中框、投射物池、伤害事件、回放（如有确定性需求）。
  - 以上必须位于游戏项目或可选扩展层，不成为 P0 `GameWorld` 的内置规则。

- [!] **T6.5 — 编辑器、联网、mod 热卸载、通用 ECS DAG 并行调度、完整 3D。**
  - 在已有真实游戏需求、性能证据和稳定的 Scene/资源协议前不启动。

## 7. 执行顺序、依赖与里程碑

```text
已完成：T0.1 / T0.2 / T0.3；A0.1 / A0.2 / A0.3；M1 = T1.1 / T1.2 / T1.3；M2 = T2.1 / T2.2 / T2.3；T2.4；T1.4；T3.2

M1 可见实体：T1.1 -> T1.2 -> T1.3                    ✅（另加 T1.4 插值）
M2 可加载关卡：M1 -> T2.1 -> T2.2 -> T2.3            ✅（另加 T2.4 图集元数据）
M3 可玩闭环：M2 + T3.1 + T3.2 -> T3.3            T3.2 ✅，T3.1/T3.3 待做
M4 P0 可交付：M3 -> T4.1 / T4.2 / T4.3
M5 内容与世界：M4 -> T5.1 / T5.2 / T5.3 -> T5.4
M6 规模与表现：M5 -> T6.1 / T6.2 / T6.3
```

### 当前下一项

**M1 可见实体（2026-10-25 完成）**：A0.1/A0.2/A0.3 三个硬门通过；T1.1（六个组件 + 准入规则 + `Emplace` 四种
形态）、T1.2（`Atom_Scene2D` + `Renderer2D` 翻转 + 相机映射 + 提交前排序）、T1.3（`Screen::BeginFrame` 帧节奏桥 +
`Runtime/GameScreen` + `Example_2D_Smoke` 实机跑通）。

**M2 可加载关卡（2026-10-29 完成 T2.1 + T2.2 + T2.3）**：`Docs/Plans/Scene-Schema-v1-CN.md` 冻结 v1 格式（JSONC、未知
字段一律拒绝、错误诊断格式、与 ECS 组件的唯一映射）；`Utilities/Json` 提供通用 JSONC 读取器（`Atom_Json`），`Atom_Scene` 提供
`Scene/SceneSchema`、`Scene/SceneLoader`，原子切换走 `GameWorld::ReplaceEntities`；
`Example_2D_Smoke` 的房间现在全部来自 `Example/Game/Assets/scenes/smoke_room.scene.json`。CTest 72/72。

**未完成的已知项（不阻塞 M3，但需在 T3.1/T3.3 前处理）**：

- G0.3 的剩余一项：**旋转精灵**（`Renderer2D` 没有旋转视图，也没有旋转四边形）。最近邻采样器已随 T2.4 落地，
  图集里写 `filter: "nearest"` 就会让整张表用 Nearest sampler 绘制。
- `RenderTransform2D` 目前是纯拷贝，固定步与显示帧之间没有插值（需要 previous-state 缓冲；`alpha` 时钟里已有）。
- **动画**（T5.3）：帧序列、状态机、帧事件。图集元数据（T2.4）已经把"帧"这一层做好了，动画只剩"按时间选帧"。
- **Example 合并不做（已决定）**：17 个 example 保持独立可执行。理由是无窗口工具（ECS/Job tour、Physics
  benchmark、Packager GUI、audio metadata）本就该独立，而 windowed demo 迁移成本高、收益被 T3.3 覆盖。T3.3 的
  vertical slice 直接长在 `Example/Game` 里，不另起示例。

**下一步进入 M3**：先做 `Sprite/` 图集元数据（美术管线，与场景共用 `Utilities/Json`），随后 T3.2 游戏输入动作边界 →
T3.1 物理集成（先按 G0.4 扩契约：sensor/user-data/查询）→ T3.3 第一个 vertical slice。

## 8. 明确禁止的提前扩张

- 在 `GameWorld` 已有固定顺序的情况下，先做通用 DAG/自动并行系统调度。
- 在真实 Scene 已加载之前，设计嵌套 Prefab 图、任意对象序列化或完整编辑器。
- 在同步 WorldSession 还未验证前，实现异步物理、全世界常驻或无边界流送。
- 将 SDL、SDL_GPU、EnTT 或具体物理 backend 类型泄漏到 Scene、ECS 或游戏组件公共 API。
- 为了支持未来手柄而让 P0 玩法直接消费 SDL 原始输入事件。

这些约束不是缩小目标，而是保证每一层均由可运行的前一层验证，最终能成长为通用 2D 世界引擎，而非只由规划拼成的框架。

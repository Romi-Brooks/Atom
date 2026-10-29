# Atom 源码架构地图

本页是新贡献者阅读仓库的入口。目录按“谁拥有概念与生命周期”划分，而不是按某次实现所使用的第三方库划分。

## 先从哪里读

1. 从 `Runtime/README-CN.md` 了解应用、Screen、GameWorld 和帧调度。
2. 从 `Window/README-CN.md` 了解窗口/渲染宿主与 Runtime 的边界。
3. 从 `Backend/README.md` 了解 contract、具体 backend 与 composition 的边界。
4. 再按需要进入 `Render/`、`Media/`、`Physics/`、`UI/` 与 `ECS/`。

## 目录职责

| 目录 | 负责什么 | 不负责什么 |
|---|---|---|
| `Algorithm/`、`Time/`、`Concurrency/`、`Error/`、`Log/` | 最低层值类型、服务与诊断 | SDL/GPU、游戏对象生命周期 |
| `ECS/` | entity、纯组件数据与查询 | 资源 handle、Renderer、具体物理 backend |
| `Physics/Integration/` | ECS 与物理世界的唯一桥梁（单位换算、句柄映射、接触转值） | ECS 存储、渲染、Runtime 事件类型 |
| `Physics/` | shape、physics contract、solver 与可替换 physics backend | 窗口、渲染资源 |
| `Media/Audio/` | 音频资源、播放逻辑与格式 codec | 具体平台播放设备的选择 |
| `Render/`、`UI/`、`Layout/` | 绘制、文本、UI 和布局 | 主循环、SDL 细节 |
| `Render/Scene2D/` | ECS 精灵与 2D 相机的收集、排序、提交 | 世界本身、主循环、帧节奏 |
| `Sprite/` | 图集元数据：一张纹理 + 命名帧 + 整表 filter | ECS、渲染、动画 |
| `Utilities/Json/` | 严格 JSONC 读取器（行号列号诊断） | 场景语义、组件映射 |
| `Scene/` | 场景文档的 schema、校验与加载器 | Runtime 帧循环、渲染、具体格式语法 |
| `Example/TestGame/Assets/` | `TestGame` 的内容根（场景/prefab/图集/占位贴图），开发期挂到 `res://`；贴图由同目录 `generate_textures.py` 生成 | 引擎实现、仓库共享资源根 |
| `Test/Data/` | **故意非法**的夹具（坏版本、重复 id、未知字段），只被对应测试读取 | 游戏内容根；它们必须永远加载失败 |
| `Window/` | 后端无关的窗口/帧宿主门面 | SDL3/SDL_GPU 的具体实现、Renderer2D/UI 绘制策略 |
| `Runtime/` | Application、Screen、GameWorld、GameScreen、调度与生命周期 | Renderer2D/GPU 后端实现 |
| `Backend/Contracts/` | backend 无关接口与基础传输类型 | 领域实现、SDL/GPU 类型 |
| `Backend/SDL3/`、`Backend/SDLGPU/` | 具体平台与 GPU 实现 | 游戏规则与 Screen |
| `Backend/Composition/` | 默认 backend 注册、选择与装配 | 游戏帧循环 |
| `Debugger/` | 共享 overlay 基础设施；`Panels/` 放可挂载诊断面板 | 业务插件或运行时扩展机制 |
| `Example/`、`Test/` | 能力验证与回归 | 可复用引擎实现 |

## 依赖方向

```text
基础层 → Contracts / 领域纯数据 → Render · Media · Physics · ECS
                                  ↓
                            Window / Runtime
                                  ↓
                              Example / Test

具体 backend ──实现──> Backend/Contracts
Backend/Composition ──装配──> 具体 backend
```

箭头表示可依赖方向。`ECS`、`Runtime`、`Window` 不应反向依赖具体 SDL、GPU 或资源实现；`Backend/Contracts` 不应反向包含 Physics、Filesystem 或 Algorithm 的高层定义（A0.2 已收敛：Contracts 只依赖 `Atom_Math`/`Atom_Color`/`Atom_Input`/`Atom_FS` 四个低层 target）。

```text
Atom_Math / Atom_Color     纯值类型（叶子）
Atom_Error / Atom_UTF8     错误与编码（叶子；Atom_Error 不依赖 SDL）
Atom_FS → Atom_Error
Atom_Backend_Contracts → Atom_Math Atom_Color Atom_Input Atom_FS
Atom_Algorithm → Atom_Math                       （变换/相机，不再依赖 Contracts）
Atom_ECS → Atom_Error Atom_Log Atom_Math Atom_Color
Atom_Scene2D → Atom_ECS Atom_Renderer2D Atom_Render_Resources   （不依赖 Runtime）
Atom_Runtime → Atom_Window Atom_Display Atom_Time Atom_Log Atom_ECS
```

## CMake 规则

- 新模块链接最小、显式的 domain target；不要为省事链接 `Atom_All`。
- `Atom_All` 只是应用入口的 convenience package；`Atom_Core` 仅保留兼容，不接受新使用者或新依赖。
- `Atom_Backend_Composition` 与 `Atom_Backend_Render_Composition` 是正式 target；同名含 `Runtime` 的旧 target 仅为兼容 shim。
- public header 直接 include 的任何 target，都必须以 `PUBLIC` 或 `INTERFACE` 依赖表达。
- 新游戏功能先进入对应领域模块；若只为验证能力，进入 `Example/` 或 `Test/`，不要在两者中沉淀通用实现。

## Window 与 Render 的边界

`Window/RenderWindow` 不能迁入 `Render/`。它除持有渲染帧边界外，还负责原生窗口的标题、关闭请求、事件泵、resize
稳定化、VSync 配置以及 selected backend 的宿主访问面；这些职责在纯 headless renderer 或未来非 SDL 平台窗口下仍然存在。
`Render/` 应只拥有绘制政策和资源，例如 Renderer2D、Text、Sprite/Scene2D、绘制批次和渲染资源。

目前唯一值得后续评估的是名称：若 `RenderWindow` 持续扩展为应用级宿主，可改名为 `RenderHost` 或拆分为 `WindowHost` 与
`FrameHost`；这属于公开 API 迁移，不能为整理目录而抢先进行。

## Physics 可替换实现的目录

Physics 需要承载自研 solver 和未来 Box2D/Jolt 等可替换实现，因此 implementation 子目录本身是合理的；不能把它们重新
放到根级 `Backend/`，否则 Physics 的领域所有权会再次丢失。当前 `Physics/Backend/Internal` 功能归属正确，但名称中的
单数 `Backend` 容易与根级 `Backend/` 混淆。

待下一次**仅重命名、无行为变化**的目录整理时，建议统一为 `Physics/Backends/Internal`（复数表示 provider 集合）；届时
同步修改 include、CMake、文档和测试。不要为这次命名调整改变 `IPhysics*` contract 的归属；contracts 仍应留在
`Physics/Contracts`。

## 不要机械复制 `Backends/`

目录应按真实的变化轴命名。只有存在稳定 contract、并确实要支持多个 provider 时，才引入 provider/implementation
目录；否则只会多一层空壳。

| 模块 | 判断 | 推荐组织 |
|---|---|---|
| `Physics/` | 有 `IPhysicsBackend`，Internal 与未来 Box2D 是同一替换点 | `Contracts/` + `Backends/<provider>/`；已记录重命名待办 |
| `Media/Audio/` | 格式解析变化轴不同于播放设备 | 保持 `Codecs/<format>/`；SDL3/Mixer 播放设备留在根级 `Backend/SDL3/Audio/` |
| `Render/` | GPU device/window backend 跨越 Window、Render、ImGui，且由平台实现 | 不在 `Render/` 再建 `Backends/`；保留根级 `Backend/SDL3/`、`Backend/SDLGPU/`，Render 只放 backend-neutral 策略与资源 |
| `Filesystem/` | `Native`、`Memory`、`Package`、`Overlay` 是 VFS mount 类型，不是硬件/平台 backend | 当前规模可保持扁平；当再增加至少一个 mount provider 时，迁为 `Contracts/` + `Mounts/{Native,Memory,Package,Overlay}`，不使用 `Backends/` |
| `Media/Image/` | 目前只有一条 image decode 路径 | 暂不建目录；第二种独立格式/库实现出现时再建 `Codecs/` |
| `Input/` | ActionMap/InputSystem 是游戏输入语义，平台采集属于 SDL backend | 保持现状；手柄支持扩展 binding source，不在 `Input/` 建 platform backend |
| `Asset/` | Loader 是按资源类型分工，不是可替换底层 provider | 当前保持；出现三种以上同类 loader 时再按 `Loaders/` 归类 |

这条规则也适用于未来模块：先确认“替换的是谁、由谁选择、contract 属于谁”，再决定目录，而不是先创建一个叫
`Backend` 的文件夹。

## 已知的坑（都踩过一次，别再踩）

### 1. 窗口启动时不要开文本输入

`SDL_StartTextInput` 会在 Windows 上挂上输入法：字母键被 IME 吃掉，SDL 根本不产生
`SDL_EVENT_KEY_DOWN`。表现是"鼠标能用、画面正常、键盘完全无效"，而所有单元测试都是绿的——
因为它们直接给 `InputSystem` 喂事件，从不经过窗口。

现在的规则：**文本输入默认关闭**（`IWindow::SetTextInputEnabled(false)`），只有持有焦点的文本框才把它打开。
需要输入文字时调用 `RenderWindow::SetTextInputEnabled(true)`，失焦/关闭时立刻设回 false。

### 2. 单例必须有可用的初始状态

`GameInput` 曾经要求启动时调用 `ApplyDefaultBindings()`，但没人调用，于是动作表是空的：
每个动作查询恒为 false、`FindAction` 恒失败，所有按键似乎都"失灵"。

现在的规则：**单例在构造时就进入可用状态**。需要在运行期改配置（比如换绑定）是另一回事，但"能玩"
不能依赖某个调用方记得调一次。

### 3. 按键自动重复必须由引擎过滤，不能交给每个 Screen

按住一个键时，操作系统大约每秒重复 30 次，每次都是 `EventType::KeyPressed`（`KeyEvent::is_repeat = true`）。
一个"按下就切换"的 Screen 会**每秒切换 30 次**：按住 Space 暂停会自己抖回来、按住 R 会连续重载关卡，
而画面看起来只是"卡了一下"。

现在的规则：`ScreenManager::HandleEvent` 在把事件交给 Screen **之前**丢弃 `KeyPressed && is_repeat`。
`InputSystem` 仍然保留 repeats（`Held` 必须为真，边缘查询自己会过滤），所以"按住"和"按下这一帧"都正常。
新增的 Screen 不需要、也不应该自己再判断一次。回归测试：`Atom_Runtime.ScreenManager` 的 repeat 小节。

### 4. 组件只放配置，不放"集成状态"

`ecs::PhysicsBody2D` 曾经有一个 `handle` 字段，注释说是"engine-side body handle"，实际被写死成 `1`、全仓无人读。
它让"这个组件能告诉我 body"看起来成立，而真值在 `Physics/Integration` 的映射表里。

现在的规则：**组件只有配置**；实体↔资源的映射属于集成层。否则下一个人会信那个字段（见 RI-003）。

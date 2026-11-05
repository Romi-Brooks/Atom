# Atom 面向简易 2D 游戏的就绪度评估与路线图

> 状态：架构评估与实施建议；其中 Runtime 与游戏 UI P0 已落地，其余项目仍待实施。
>
> 更新日期：2026-10-22
>
> 范围：以“可交付的单机简易 2D 游戏”（启动、进入关卡、核心玩法、HUD/菜单、重新开始或切关、打包运行）为目标；不预设联网、编辑器或 3D。

## 1. 总结

Atom 已具备良好的引擎底座：窗口与固定步时钟、输入、2D 批渲染、资源/VFS/打包、音频、ECS 存储、Lua 与 2D 物理均已落地。

当前缺失的不是某一个绘制 API，而是将这些基础设施稳定组合为**游戏世界、游戏循环和内容**的中间层。因此动画与粒子渲染是重要能力，但不是首要缺口；先让一个实体能被加载、更新、碰撞、绘制并通过 UI 完整开始/结束一局，动画和粒子才有可靠的宿主。

## 2. 已有能力与边界

| 领域 | 当前能力 | 尚不等于游戏能力的部分 |
|---|---|---|
| 窗口/时间/输入 | `Runtime::Application` 主循环/时间域/Screen 调度，`RenderWindow` 窗口与帧提交，键鼠输入与 ActionMap | 没有加载策略、手柄输入与设置持久化 |
| 游戏 UI | `Atom_UI` 保留模式控件树、Yoga 布局、屏幕坐标 Canvas、Panel/Label/Button、命中/焦点/鼠标捕获/模态层 | 无 TextField/IME、滚动、Image/九宫格、手柄导航、主题与本地化/字体回退 |
| 2D 渲染 | 纹理、source rect/atlas、批处理、基础 camera、layer、clip、文本、后处理 | 没有 Sprite、Camera、Tilemap、RenderSystem 等游戏侧抽象；旋转/origin、render texture 仍待补 |
| 资源 | VFS、APKG、Texture/Script/Audio 的资源句柄与缓存 | 无依赖图、异步加载、热重载、内存预算、内容导入/cook |
| ECS | 安全实体句柄、组件存储、确定性遍历基础 | 无系统调度、事件、序列化、多线程；尚无任何行为系统消费它 |
| 物理 | 2D 刚体、接触、关节、固定步、确定性窄相 job 并行 | 无 ECS 桥、游戏碰撞层/触发器/查询接口；CCD、复杂形状与更多关节尚未具备 |
| 音频/脚本 | 音乐、SFX、流式播放、Lua Host | 缺游戏音频事件/总线；Lua 主要绑定音频，未形成实体/场景脚本层 |
| 调试 | Log、ImGui Overlay、FPS/Job 面板 | 缺 profiler、帧统计、实体检查器、碰撞可视化、资源统计 |

## 3. 缺失模块与优先级

### P0：形成最小可玩闭环

1. **运行时组合层（Runtime）**
   - 明确初始化、帧阶段、暂停、退出、失败回滚与 shutdown 顺序。
   - 只允许一个帧循环拥有者；由 `Runtime::Application::Run()` 驱动，`RenderWindow` 只提供窗口、事件泵与帧提交，不再创建第二个 while-loop。
2. **ECS 游戏系统**
   - 最小组件：`Transform2D`、`SpriteRenderer`、`Camera2D`、`PhysicsBody`/`Collider`、`Script`、`Lifetime`。
   - 最小系统：输入/玩法、物理同步、动画（以后）、渲染提交、寿命回收。
3. **Scene 与 Prefab**
   - Scene 描述世界实体和资源引用；Prefab 是可复用实体模板与覆盖值；两者均需版本号和明确的资源 URI。
   - 最小实现先覆盖引擎自有 POD 组件，拒绝未知组件并给出诊断；不在第一版做任意 C++ 对象序列化。
4. **游戏 UI**
   - **P0 已完成**：`Atom_UI` 已将 Layout、Renderer2D、InputSystem 组合为 Panel/Label/Button、HUD/菜单/暂停层、焦点/鼠标命中与模态输入阻断；`Example_UI_Runtime_Demo` 演示 Screen 页面切换与暂停叠加。
   - TextField 与 IME 组合文本、滚动、Image/九宫格、键盘/手柄导航、主题/本地化留给 P1；不要把这些较高复杂度项伪装成 Button P0 的一部分。
5. **游戏物理桥**
   - 物理 world 与 ECS entity 的双向同步、碰撞/触发事件、销毁时序、像素/米换算及 debug draw。

### P1：完成典型 2D 内容生产

- SpriteSheet/Atlas 元数据、逐帧动画、Animator 状态机、帧事件、翻转、pivot。
- Tilemap/对象层、图块碰撞生成、地图加载（平台动作、RPG、解谜高度依赖）。
- Camera 跟随/边界/平滑、视差、屏幕震动与场景转场。
- 存档、设置、键位重映射；游戏状态版本迁移。
- 游戏化音频事件、Master/Music/SFX/UI bus、音效池与场景切歌。
- Profiler、帧统计、实体/资源/物理检查工具。

### P2：表现、规模与产品化

- 粒子发射器、生命周期曲线、对象池、批渲染、预热和调试可视化。粒子不应一颗一个 ECS entity。
- 本地化、字体回退、可访问性、手柄支持。
- 异步资源加载、热重载、资源依赖图和内存预算。
- 按游戏类型决定 AI/寻路、联网、回放、mod 支持或复杂物理。

## 4. 推荐实施顺序

```text
Runtime 的帧阶段与生命周期
  -> Transform / SpriteRenderer / Camera + RenderSystem
  -> Scene / Prefab + 最小关卡加载
  -> PhysicsSystem + 输入驱动的一项核心玩法
  -> HUD、菜单、暂停与重新开始
  -> 动画 + Tilemap
  -> 存档、音频事件、开发诊断
  -> 粒子与视觉打磨
```

建议以一个很小的 vertical slice 验收：启动后进入一个场景；加载角色和障碍物；输入驱动角色；发生碰撞；绘制 HUD；暂停、重新开始及退出均不泄漏资源。不要先实现通用编辑器、联网或完整异步物理。

## 5. Runtime 的最小设计

Runtime 不是“包办一切的全局单例”，而是一个可显式构造、可测试的组合层。推荐新建 `Runtime/` 模块：

```text
Runtime/
  Application.hpp/.cpp      // 初始化、Run/RequestStop、失败回滚、shutdown
  GameWorld.hpp/.cpp        // 一个 EntityWorld、scene 状态、系统集合
  System.hpp                // 明确的 phase 接口和依赖
  SystemScheduler.hpp/.cpp  // 固定顺序调度，未来才考虑并行
```

首期帧顺序应与现有 `RenderWindow` 对齐：

```text
事件 -> Input begin/frame state -> FixedUpdate × N
     -> Update -> Render submit -> Overlay -> Present
```

`Runtime::Application` 直接拥有这些阶段并显式调度 Screen；其 RAII observer 只供 Debugger、Overlay 等工具集成，不混用两套游戏业务调度。首期系统顺序写死并可测试，等有真实独立系统及 profiling 证据后，再引入依赖图与 JobSystem 并行。

### Runtime 难度评估

算法难度为**中等偏低**，主要风险是生命周期和职责边界，不是循环本身：

| 子项 | 难度 | 主要风险 |
|---|---|---|
| phase/系统接口与固定顺序调度 | 低 | 不要过早实现通用 DAG 调度器 |
| 资源与窗口初始化/销毁顺序 | 中 | GPU 资源、Screen、Audio、JobSystem 的停止顺序 |
| 暂停、切场景、加载失败回滚 | 中 | 不能在遍历 ECS 时立刻销毁对象 |
| 支持异步加载、热重载和多世界 | 高 | 不属于 P0，等最小闭环稳定后再做 |

## 6. ECS 系统、Scene 与 Prefab 难度评估

### ECS 游戏系统：中等

`EntityWorld` 的存储、句柄和迭代已完成，难点在于定义清楚的所有权及写入时序：

- 物理是 simulation 真源，渲染只读插值后的 Transform；不能让 PhysicsSystem 与 RenderSystem 各自写坐标。
- 系统在遍历中不立即改变实体结构；创建/销毁使用命令缓冲并在帧边界提交。
- 碰撞、动画帧、脚本调用需进入有序事件队列，避免回调重入破坏确定性。
- 首期只支持固定的一组引擎组件。当前 ECS 的显式组件/迭代实例化设计意味着“自动注册任意 mod 组件”本来就不是低成本能力，应明确推迟。

建议按 `Physics/Integration/`、`Render/Scene2D/`、未来 `Animation/` 放置系统，而非把所有行为堆入 `ECS/`。ECS 保持存储职责。

### Scene/Prefab：中等偏高

格式解析不难，真正难处是长期兼容：实体引用、资源引用、嵌套 Prefab 覆盖、循环引用、版本升级、未知字段保留以及加载失败时的原子回滚。

首期应采用受约束设计：一个可读文本格式（例如 JSON/TOML）或预烘焙二进制；`res://` 资源 URI；显式 schema version；仅支持平面 Prefab 和组件字段覆盖；加载到临时 world，成功后才切换为当前 world。嵌套 Prefab、热重载合并、编辑器 round-trip 和任意脚本对象序列化均放到后续版本。

## 7. 当前架构与目录的审查结论

### 应优先处理

1. **缺少 Runtime 边界是最主要的定位空洞。** `Atom.cpp` 只是日志入口；`RenderWindow` 同时承担窗口、主循环、Screen 调度和部分运行时职责。应新增 `Runtime/`，让 Window 保持平台/帧泵职责，让 Runtime 组合游戏服务。
2. **`Screen` 与未来 `Scene` 不能混为同一概念。** Screen 适合菜单、加载页、暂停层等呈现导航；Scene 是可序列化的游戏世界。Scene 不应塞进 `Window/`，也不能只靠 ScreenManager 替代。
3. **`Atom_Core` 聚合目标没有纳入 `Atom_ECS` 和 `Atom_Physics`。** 这对当前空入口无害，但名称会让游戏使用者误以为 Core 足以构建玩法。Runtime/Game 示例应显式依赖它们，或在 Runtime 落地时重新定义/收窄 `Atom_Core` 的语义。
4. **物理/ECS 桥必须放在 Physics 侧。** `Physics/README.md` 的职责约束是正确的；不要在 `ECS/` 放 `PhysicsSystem`，也不要让 Renderer 持有物理体。
5. **ECS 的 `ForEach` 顺序契约需要统一。** `ECS/EntityWorld.hpp` 声明它按 entity index 升序，而实现和 README 后段说明它走后端 pool order；两者不能同时成立。应以实现为准，或改为真的排序遍历，并在 Physics/Animation 的确定性约定中明确“稳定 pool order”是否足够。
6. **`Atom_Error` 的“无 backend 依赖”定位与实现不一致。** 公开头文件的注释承诺它不依赖 backend，但实现直接调用 SDL error API，且 CMake 对 SDL3 为 PUBLIC 链接。这会把 SDL 传播给所有 Error 消费者。应在 Runtime 之前厘清：要么承认 Error 是平台诊断层，要么把 SDL 错误捕获移动到 SDL backend 并仅把已捕获字符串传入 Error。

### 合理但需要保持约束

1. **`Backend/Contracts` 与 `Backend/<实现>` 的分层合理。** `Backend/Physics/Internal` 与未来 Box2D 后端并列是正确位置；不要把第三方类型放进 Physics 或 ECS 公共头。
2. **`Algorithm/Camera` 与游戏 CameraComponent 分离是合理的。** 前者是数学值类型；组件、跟随和镜头行为应落在 Render/Scene2D 或 Runtime 系统层。
3. **`Asset`、`Filesystem`、`Media`、`Render/Resources` 分开是可接受的。** 它们分别是资源身份/加载、字节来源、解码、GPU 驻留。后续只需补一份“资产生命周期”文档，防止调用方跨层直接加载文件。
4. **`Layout` 不是 UI。** 保留其独立模块；新增 UI 时由 UI 模块依赖 Layout、Renderer2D、InputSystem，反向依赖禁止。

### 可在后续重构时改善

- `Backend/Runtime` 同时包含 audio 与 render 的运行时注册，名称略泛；当 Runtime 层建立后，可将“backend 选择/注册”统一为明确子目录或保持它仅做 backend runtime，避免与游戏 Runtime 混淆。
- `Media/Image` 目前与 Audio、Video 并列，但其主要消费者是渲染纹理；无需立即移动，先用 loader/资源接口保持边界。若未来发展通用图片处理，再保留在 Media；若只服务纹理导入，则可迁至 `Asset/Import`。
- `Render/Core/RenderGraph` 与现有轻量 2D 渲染路线并存时应明确状态；未投入正式使用前，不要让游戏系统依赖它。

## 8. 不应在 P0 提前做的事项

- 通用 ECS DAG 调度器、自动系统并行、异步物理线程。
- 完整编辑器、嵌套 Prefab 图、脚本对象的任意序列化。
- 通用 RHI、复杂 3D、全屏材质系统重构。
- 联网、回放、mod 热卸载。

这些能力都可能有价值，但在一个可玩样例验证 Runtime/ECS/Scene 边界前实施，会把架构风险成倍放大。

# Atom 任务清单（唯一排期来源）

> 状态：**唯一有效的任务排期文档**（2026-10-29 重建，合并原 `TaskList-CN.md`）。
> 本文件只回答三件事：**现在能做什么、下一步做什么、为什么是这个顺序**。
>
> 分工：
> - 缺陷与技术债 → [`../Remaining-Issues.md`](../Remaining-Issues.md)（编号 ARCH-*/CORE-*/CONCURRENCY-*）
> - 格式与契约（场景、图集、组件准入）→ [`Specs/`](Specs/)
> - 子系统设计来龙去脉 → [`Subsystem-Plans-CN.md`](Subsystem-Plans-CN.md)
> - 目录职责与依赖方向 → [`../Architecture-Map-CN.md`](../Architecture-Map-CN.md)
>
> 历史细节（每项任务当初怎么推、踩了什么坑）不进本文件，留在 git log 与 commit message 里；本文件只保留
> 结论与验收依据，否则它会继续膨胀成现在这个 500 行的样子。

## 0. 优先级定义

| 级别 | 含义 | 触发条件 |
|---|---|---|
| P0 | 阻塞交付 | 不解决就无法声明"P0 完成"；CI 红、崩溃、数据竞争、不能重复构建 |
| P1 | 结构与能力缺口 | 影响正确性或后续任务可行性（边界、生命周期、缺失子系统） |
| P2 | 能力与体验 | 玩法/表现/工具链增强，可在 P0 之后并行 |
| P3 | 工程化 | 测试基线、许可、打包、文档 |

---

## 1. 现状：已经能做什么

一条命令构建、一条命令测试；**78 个 CTest 全绿**（`ctest` 在 `cmake-build-debug` 下运行）。

```powershell
& 'D:\Program\JetBrains\CLion\bin\cmake\win\x64\bin\cmake.exe' --build cmake-build-debug --parallel
cd cmake-build-debug; & 'D:\Program\JetBrains\CLion\bin\cmake\win\x64\bin\ctest.exe' --timeout 90
.\cmake-build-debug\Example_2D_Smoke.exe      # 可玩的小房间：WASD/E/Space/R
```

| 能力 | 状态 | 验收依据 |
|---|---|---|
| 帧节奏（固定步 + 可变步 + 插值 alpha） | ✅ | `Atom_Runtime.GameScreenFrame`、`Example_2D_Smoke` 实机 |
| ECS 组件词汇与查询实例化 | ✅ | `Specs/ECS-Component-Vocabulary-CN.md`、`Atom_ECS.*` |
| 2D 渲染（批处理、排序、翻转、最近邻） | ✅ | `Atom_Scene2D.SpriteRenderer` |
| 场景文档 v2（JSONC、原子切换、prefab、图集、物理） | ✅ | `Specs/Scene-Schema-CN.md`、`Atom_Scene.*` |
| 输入动作层（轴、单次触发、玩法闸门） | ✅ | `Atom_Input.ActionState`、`Atom_Input.InputSystem` |
| 物理↔ECS 桥（单位换算、sensor、接触事件） | ✅ | `Atom_Physics.Physics2DWorld`、`Atom_Physics.SensorAndUserData` |
| 可玩闭环（启动→移动→触发→暂停→重开） | ✅ | 人工按键验证（附录 A）+ repeat 过滤回归测试 |

**已完成任务**：T0.1–T0.3、A0.1–A0.3、G0.1–G0.5、T1.1–T1.4、T2.1–T2.4、T3.1a/T3.1b、T3.2、T3.3。

---

## 2. P0：现在就该做（M4 可交付）

### T4.2 — CI 真正跑测试（**最先做，半天**）

- 现状：`.github/workflows/build.yml` 两个平台**只 `cmake --build`，从不执行 ctest**。78 个测试写了却没人跑，
  "CI 绿"目前只等于"能编译"。
- 前置（必须先做）：**[RI-001 / RI-002 / RI-015](../Remaining-Issues.md) 三个偶发超时要先定案**——一个会随机红的门禁
  比没有门禁更糟，它训练所有人忽略红灯。
- 工作：两个 job 各加一次 `ctest --output-on-failure --timeout 90`；Linux job 需要 `xvfb` 或确保测试不建窗口
  （目前测试确实都不建窗口，需复核）。
- 验收：任一平台的 schema / 物理同步 / 排序回归都能阻断合并。
- 顺带：把 vertical slice 抽成静态库，让独立 test target 走**同一条**窗口事件路径（本轮两个 bug 就是因为
  测试绕过了窗口路径而变绿）。

### T4.3 — 线程与生命周期的自动化回归

- 工作：反复 load/restart/unload scene（`R` 键按 100 次），断言 body 数、texture handle、事件队列长度不增长。
- 验收：ASan/UBSan 可用平台纳入 CI；无悬垂句柄、无残留 body、无用的已销毁 GPU 资源。
- 依据：`SmokeScreen::LoadRoom` 每次重建 `Physics2DWorld` 与 1000 个背景实体，目前只靠人工观察。
  见 [RI-005](../Remaining-Issues.md)（`bodies_` 每步重建）与 [RI-006](../Remaining-Issues.md)（测试未覆盖速度 API）。

### T4.1 — 无窗口 / Recording 渲染后端

- 工作：把 RenderSystem 的排序、相机变换、frame/pass 顺序、资源生命周期做成可断言对象，不靠截图。
- 依赖：T1.2（已具备）。
- 备注：截图只能证明"某次跑起来是这个样子"，无法防回归；本任务完成后 `Example/Game/smoke-scene.png` 才只需
  人工看一眼。

### P0 收尾判据

"P0 完成"= 上表 §1 每个能力都有自动化验收 + CI 在 Windows 与 Linux 都执行这些验收 + 人体工学上一条命令可玩。
目前差 T4.1/T4.2/T4.3 三项。

---

## 3. P1：内容与世界（M5）

按依赖顺序，**不要跳**：

| 任务 | 内容 | 依赖 | 为什么在这个位置 |
|---|---|---|---|
| **T5.1** | 正交 Tilemap + 对象层 + tileset 元数据 + 图块碰撞 | T1.2/T2.2/T3.1 | 现在示例用 1000 个实体手搓地板（`AddBackdrop`），大关卡的第一道墙就是它 |
| **T5.3** | 逐帧动画（clip、状态机、帧事件） | T2.4 | 图集已把"帧"这一层做完，动画只剩"按时间选帧"；帧事件必须进 `GameEventQueue` |
| **T5.2** | Camera 行为层（跟随、边界、平滑、视差、震动、转场） | T5.1 | 现在跟随逻辑写死在示例的 `camera-follow` 里；边界与视差要等关卡变大才有意义 |
| **T5.4** | `WorldManifest` / `WorldSession` / 同步切块 | T2.2/T5.1 | "星露谷式世界"的第一个引擎级能力；先同步，再流送 |
| **T5.5** | 存档、设置、音频事件（bus/音效池/切歌） | T5.4 | 存档要记录 game 定义的持久 id，必须先有 Scene/Chunk 的 stable id |
| **T5.6** | 开发诊断（实体检查、碰撞可视化、资源统计、帧耗时） | T5.1 | 没有它，T5.1 之后的问题只能靠猜 |

### P1 里还剩的架构账（本轮审计，已修 3 笔）

- ~~RI-003/RI-004/RI-005/RI-007~~：已修（假字段、velocity 契约、每步重建、Debugger 过度公开）。
- **RI-008（未修）**：`Atom_Text PUBLIC Atom_Assets` ——公开头 `FontFileLoader.hpp` include 了 `Asset/`，
  所以不能直接改 PRIVATE；要把实现挪进 `.cpp` 才能解耦。

---

## 4. P2：规模与表现（M6）

| 任务 | 内容 | 前置条件 |
|---|---|---|
| **T6.1** | 异步资源加载与 chunk 流送（依赖图、取消、预算） | T5.4 同步语义稳定 |
| **T6.2** | 粒子系统（emitter 挂实体，粒子用连续存储，不进 ECS） | T5.2 |
| **T6.3** | 手柄 binding、键位重映射 UI、可访问性（字体 fallback、缩放） | T3.2 的动作层已就绪，只差第二个 input source |
| **T6.4** | 类型扩展包（横版单向平台/coyote time、俯视交互/日程、弹幕命中框） | 必须在**游戏侧**，不得进 P0 引擎规则 |

### P2 里的小额技术债

- **RI-014a–f**（性能与契约细节，见 Remaining-Issues）：Collect 的二次 ECS 查询、每步分配的容量丢失、
  HUD 每帧字符串、1000 个背景实体、kinematic 晚一帧、slice 端到端检查缺位。
- **G0.3 剩余**：旋转精灵（`Renderer2D` 没有旋转视图/旋转四边形）。最近邻采样已随 T2.4 落地。
- **RI-009**：`ActionState::Movement()` 把"y 向下 + 只有四个数字方向"写进了输入层；非渲染用途的宿主会继承
  渲染器约定。保留 `Axis()`，把 `Movement()` 的注释讲清楚（或改为 `Axis2D` 返回中性向量）。
- **RI-010**：`IWindow::SetTextInputEnabled` 目前零调用方，契约没有消费者也就没有测试。
- **RI-011**：`SceneLoaderTest` 把示例房间的实体/精灵数写进引擎测试——改一次关卡内容就要改引擎测试。

## 5. 暂缓（明确不做）

| 项 | 暂缓理由 |
|---|---|
| T6.5 编辑器 / 联网 / mod 热卸载 / 通用 ECS DAG 并行调度 / 完整 3D | 缺少真实游戏需求、性能证据与稳定的 Scene/资源协议 |
| Example 合并成一个大示例 | 已决定不做：无窗口工具本就该独立，windowed demo 迁移成本高、收益已被 T3.3 覆盖 |
| 3D 物理 / CCD / 复合形状 | `IPhysicsBackend` 已能容纳；等有需求再做 Box2D 后端（M7 可选） |

---

## 6. 里程碑

```text
M1 可见实体：T1.1 -> T1.2 -> T1.3 (+T1.4 插值)          ✅
M2 可加载关卡：M1 -> T2.1 -> T2.2 -> T2.3 (+T2.4 图集)   ✅
M3 可玩闭环：M2 + T3.1 + T3.2 -> T3.3                   ✅
M4 P0 可交付：M3 -> T4.2 -> T4.3 -> T4.1                ← 现在在这里
M5 内容与世界：M4 -> T5.1 -> T5.3 -> T5.2 -> T5.4        (T5.5/T5.6 并行)
M6 规模与表现：M5 -> T6.1 / T6.2 / T6.3
```

**下一步只有一个动作**：修掉 [RI-001/RI-002/RI-015](../Remaining-Issues.md) 的偶发超时，然后给 CI 加 `ctest`（T4.2）。
在 CI 会跑测试之前，后面每个任务都是在"不知道有没有回归"的状态下推进。

---

## 7. 决策记录（只记结论，理由见 git log 与子系统文档）

| 决策 | 结论 | 出处 |
|---|---|---|
| 场景格式解析器 | 自研 JSONC 读取器（方案 A），不用第三方 | `Specs/Scene-Schema-CN.md` §8 |
| 场景版本策略 | 新增字段必须递增版本；loader 只接受声明集合 `{1,2}`，集合外整份拒绝 | `Specs/Scene-Schema-CN.md` §10 |
| 组件准入 | 只有纯数据可进 `ATOM_ECS_COMPONENTS`（R1–R5） | `Specs/ECS-Component-Vocabulary-CN.md` |
| 物理与 ECS 关系 | 独立 world + 轻量组件壳 + 唯一集成层换算单位 | `Subsystem-Plans-CN.md` §C |
| 固定步插值 | 显示副本 = 上一步；混合只进 draw item，绝不写回组件 | `Subsystem-Plans-CN.md` §D（P4） |
| 游戏输入 | 引擎提供 `ActionState`，动作枚举属于游戏；暂停走事件路径 | `Input/ActionState.hpp` |
| 文本输入 | 默认关闭（IME 会吃掉字母键），只有焦点文本框打开 | `Backend/Contracts/Window/IWindow.hpp` |
| 示例合并 | 不做 | 本文 §5 |

---

## 附录 A：为什么本轮的验证方式变了

T3.3 第一次交付时，我用一个 `--selftest` 命令行开关做无窗口验证，它通过了，但游戏里键盘完全没反应。
两个原因（动作表从未初始化、窗口开机就挂 IME）都不经过它验证的路径，所以它测的是自己的假设。

现在的规则：**验证必须覆盖真实入口**。能用自动化覆盖的写测试；只能人工的，明确写下"人工验证了什么"，
并在 T4.2 里补上真自动化的路径。

同一轮里还修了 `ScreenManager` 的按键自动重复过滤（RI-013）：按住 Space 会以 30Hz 反复切暂停。这条人工
"按一下试试"发现不了，因为按住不放时画面只是顿一下——**它只能由"事件语义"层的测试发现**。

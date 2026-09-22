# ATOM 时间系统实施计划（CORE-001）

> 关联：`Remaining-Issues.md` 的 CORE-001。本文记录分域时钟架构、已落地的 P1/P2，以及后续 P3/P4。

## 结论摘要

| 问题 | 结论 |
|---|---|
| 是否自研高精度时钟？ | **否**。母钟绑定 Backend 的 `ITimeSource`，SDL3 实现用 `SDL_GetPerformanceCounter`。 |
| Sim 是什么？是否和 Screen/物理耦合？ | Simulation。**不共享一个 Sim 钟**。`game` / `physics` 是独立 DomainClock，可各自 pause/rate/步长。 |
| 音频硬件时钟？ | **采纳**。P3 用声卡采样钟 `SyncTo`（软 PLL），支撑卡点与状态切换。见下文 P3。 |
| 本期范围 | **P1 + P2 已实现**；P3/P4 本文留档，后续跟进。 |

## 1. 时钟源：SDL 绑定 vs 自实现

| 维度 | SDL `ITimeSource`（已选） | 自实现 QPC/chrono |
|---|---|---|
| 精度 | QPC 级（Windows）/ 等价高精度计数 | `steady_clock` 多数平台也是 QPC，但需自证 |
| 跨平台 | SDL 已抽象 Win/macOS/Linux/主机 | 需 `#ifdef` 或依赖 chrono 质量 |
| 与音视频时间戳 | 同一 SDL 时间体系，便于对齐 | 可能与 SDL 内部时钟漂移 |
| 架构一致性 | 符合 `Backend/Contracts` → 领域模块 | Time 直接绑系统 API |
| 可测试性 | `ManualTimeSource` / `SteadyTimeSource` 注入 | 同样可注入，默认源仍是系统钟 |
| 维护成本 | 复用 SDL | 自己维护各平台 counter |

**决策**：`Backend/Contracts/Time/ITimeSource.hpp` 为契约；`Backend/SDL3/Time/SDL3TimeSource` 绑定 `SDL_GetPerformanceCounter`；测试用 `Time/SteadyTimeSource.hpp` 的 `ManualTimeSource`。

## 2. 分域时钟（避免 Sim 耦合）

Sim = **Simulation**。Screen/Game 与 Physics **不共用一个钟**：

```text
MasterClock（单调、不可暂停、纳秒）
 ├─ game     固定步 1/60 · Screen 模拟 / 确定性逻辑
 ├─ physics  固定步 1/120 · 独立世界，可单独 pause
 ├─ render   可变步 × rate · 动画 / shader time / 插值
 ├─ ui       真实时间 · Debugger、resize settle
 └─ audio    可变步 + 负 offset（延迟补偿）· 可 SyncTo 设备
```

暂停/慢动作可以同时设在多个域上（外部统一 `SetPaused`），但不要把 Physics 和 UI 绑死在同一 DomainClock。

## 3. 帧序（CORE-001）

```text
Event → FixedUpdate × N (game) → Variable Update (render) → Render → Present
         └ physics 在自己的 domain 上跑 0..M 步
```

- `Screen::FixedUpdate(float)` / `RenderWindow::AddFixedUpdateListener`
- `Screen::Update(float)` 仍为可变步（动画、UI）
- 追帧上限 `max_steps_per_advance`（默认 5），超出 backlog 丢弃，防死亡螺旋
- 固定步 `alpha` 供渲染插值（P4 使用）

## 4. API 入口

```cpp
auto& time = atom::time::TimeSystem::GetInstance();
// Initialize 由 RenderWindow 绑定 backend_->TimeSource()
auto* physics = time.FindDomain(atom::time::domain::kPhysics);
for (u32 i = 0; i < physics->LastTick().steps; ++i)
    world.Step(atom::time::ToFloatSeconds(physics->LastTick().fixed_step_ns));
```

## 5. P3 — 音频硬件时钟（待做）

**目标**：音频中间件以声卡采样钟为准，支持卡点（beat sync）、状态切换、低延迟 A/V 对齐。

**设计要点**：

1. 音频设备提供 **采样计数**（frames played / SDL audio stream delay）作为设备时钟。
2. `audio` DomainClock 维持 `offset = -output_latency`，使 `Now()` 表示**耳机/喇叭上的播放时刻**。
3. 周期 `SyncTo(device_now, drift_gain)` 软 PLL：
   - `device_now` = 已播放样本帧 / 采样率（映射到共同 epoch）
   - 小漂移只调 `offset`，大跳变（seek/设备重开）硬对齐。
4. 卡点 API（后续）：`ScheduleAt(domain_time)` / `BeatTime` 以 **audio 域时间** 为基准，不在 render 帧上估。
5. 与 `IAudioSource::GetPlayingOffset()` 的关系：offset 是曲内位置；DomainClock 是进程内时间轴。音乐卡点用「曲内位置 + audio Now()」推导。

**待确认 / 风险**：

- SDL3 是否暴露稳定的 device delay（`SDL_GetAudioStreamQueued` 只有队列估计）。
- 多声卡热插拔后 epoch 重建策略。
- 是否需要独立 `Atom_Media` 音频中间件时钟，还是直接用 `kAudio` 域。

## 6. P4 — 渲染插值与 shader time（待做）

**目标**：去掉应用侧 `effect_time_ += delta_time`，统一从 `render` 域取时间。

1. Renderer 2D postprocess 的 `time` 改为 `render->Now()`。
2. 固定步对象用 `game`/`physics` 的 `alpha` 做状态插值，再交给 Render。
3. 可选：`render` 域支持 presentation latency offset（预测上屏时刻）。

**待确认**：

- 插值责任放 Screen 还是 ECS/Transform 层。
- 多 fixed-step 域（game 与 physics 不同步）时插值策略。

## 7. 阶段状态

| 阶段 | 内容 | 状态 |
|---|---|---|
| P1 | ITimeSource、MasterClock、DomainClock、TimeSystem | **已完成** |
| P2 | 固定步 accumulator、追帧上限、alpha、帧序接入 RenderWindow | **已完成** |
| P3 | 音频设备钟 SyncTo、卡点、A/V 对齐 | 本文档设计，待实现 |
| P4 | 渲染插值、统一 shader time | 本文档设计，待实现 |

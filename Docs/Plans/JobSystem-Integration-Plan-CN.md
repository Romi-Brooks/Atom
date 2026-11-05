# Atom 并发收尾与 JobSystem 接入规划（JobSystem Integration Plan）

> 状态：引擎并发接入的后续规划文档。JobSystem 已作为引擎 job 的唯一真源完成三站接入
> （音频元数据批量、图片批量解码、资源批量预加载）；本文件记录剩余的接入候选与并发全景收尾。
> 更新日期：2026-10-22

## 1. 已确立的并发约定（前置）

| 约定 | 结论 |
|---|---|
| 引擎内部多 job / 多线程 | **必须走自研 `atom::job::JobSystem`**，不写裸 `std::thread` |
| 短生命周期任务 | 解码、哈希、宽相、批量加载 → 是 **job** |
| 长生命周期生产者 | 音频流式解码循环、log writer → 是**专用线程（worker）**，不是 job |
| 用户侧（mod / Lua / 脚本） | 是否并行由用户自选，引擎不越界 |
| job 的唯一真源 | JobSystem；`JobDebugger` 只监控 JobSystem，即覆盖引擎全部并行 job |

已落地（保留历史，均通过 ctest）：

- **音频元数据批量读取** `AudioMetadataReader::ReadBatch`（`audio:metadata`）
- **图片批量解码** `ImageBatchDecoder::DecodeBatch`（`image:decode`）
- **资源批量预加载** `ResourceManager::TryAcquireBatch<T>`（`asset:load`，三阶段：串行查缓存去重 → 并行 load → 串行插缓存）

三者同构的**批量入口模式**：一任务一 job → 按输入序写 `results[i]` → `!jobs.IsRunning()` 时顺序回退 → `WaitAll` 阻塞。

## 2. 决策总览

| 决策点 | 结论 |
|---|---|
| 接入顺序 | Packager 哈希 → 音频 DecodeProducer → 物理并行（门槛触发）→ ThreadDebugger |
| Packager 并行粒度 | 一 asset 一 job（哈希/压缩），串行落盘保 `data_offset` 顺序 |
| 音频解码线程 | 保持专用线程（worker），只**统一抽象**消除三份重复样板，不改性质 |
| 物理并行 | 先 profiling 门槛，宽/窄相可并行，求解器（顺序冲量）保持串行 |
| 并发全景 | 先建 `ThreadRegistry`（job worker + 专用线程统一注册），再建 `ThreadDebugger` |

## 3. 剩余任务分项

### T1：Packager 文件哈希/压缩 fork-join（P2，难度中等）

**现状**：`Utilities/Packager/PackageBuilder.cpp` 的构建循环里，每个 asset 顺序执行「读源文件 → 逐块
`output.write` 拷贝 payload → 逐块 `pf::FnvaUpdate` 算 content_hash」；`data_offset` 由前序 asset 的
字节数累加而来，chunk/asset section 按 asset 顺序序列化 `content_hash`。当前 `compression_store`（store，
不压缩），哈希与写盘交织在同一循环，天然单线程。

**方案（两阶段，复用既有批量入口模式）**：

1. **阶段一（并行）**：一 asset 一 job（`packager:hash`），只读源文件逐块算 FNV `content_hash`，
   写 `results[i]`；`!IsRunning()` 顺序回退。**不读入整文件**，保持逐块流式，仅产出 hash。
2. **阶段二（串行）**：单线程按 asset 顺序写 output——累加 `data_offset`、逐块拷贝 payload、
   把阶段一已算好的 `content_hash` 写入 chunk/asset section。
3. **压缩预留**：若未来引入压缩（非 store），压缩也放阶段一（一 asset 一 job 产出压缩块 + 压缩后
   hash），阶段二串行落盘 + 写偏移。压缩块若需缓冲，评估临时文件或分块策略，避免大包整包驻留内存。

**顺序写依赖说明**：`data_offset` 与 payload 落盘顺序是包格式的硬约束，不能并行写 output；因此
「哈希可并行、写盘必须串行」是两阶段的关键分界。哈希本身与写盘解耦，并行收益来自 CPU 侧的 FNV/压缩。

**验收**：
- 并行打包结果与单线程**逐字节一致**（`content_hash` / `manifest_hash` / `header_hash` 全等）。
- `Atom_Packager.PackageBuilder` 现有测试无回归；新增「并行 == 顺序」等价断言 + 大文件集压力。
- `!IsRunning()` 顺序回退路径测试。

### T2：物理宽/窄相并行（P2，门槛触发）

**现状**：宽相（均匀网格，预留 AABB Tree）与窄相（SAT）当前单线程，逐 body / pair 遍历。

**门槛**：需先 profiling 确认宽/窄相在**真实多刚体场景**是热点；否则并行化的同步/分摊开销大于收益，
属于负优化。**未达门槛不做**。

**方案（仅当 profiling 达标）**：宽相收集 pair → 窄相一 pair 一 job 并行求接触流形（`physics:narrow`，
纯函数无共享可变状态）→ 串行按确定性顺序归并进求解器。**求解器（Sequential Impulse）保持串行**，
因为冲量求解存在跨接触的顺序写依赖，并行会破坏确定性。

**验收**：多刚体场景下结果与单线程**确定性一致**（相同 pair 顺序归并），且 profiling 有实测提升。

### T3：音频流式解码统一 `DecodeProducer` 抽象（P2）

**现状**：三处手写 `std::thread` + `DecodeLoop` 样板，功能同构、实现重复：

| 文件 | 线程成员 | 循环 |
|---|---|---|
| `Backend/SDL3/Audio/SDL3MixerStreamingSource` | `worker_` | `DecodeLoop` |
| `Backend/SDL3/Audio/SDL3MusicSource` | `decode_thread_` | `DecodeLoop` |
| `Backend/SDL3/Audio/SDL3StreamingMusicSource` | `decode_thread_` | `DecodeLoop` |

**性质**：三者都是长生命周期生产者，属**专用线程（worker）**，不是 JobSystem job——本任务**不违反**
JobSystem 约定，也不把解码循环改成 job。

**方案**：抽取 `DecodeProducer`（或同类命名）抽象，统一「启动/停止、填充 ring buffer、seek、错误上报、
进度、线程 join / 退出等待」的完整生命周期；三个 source 委托实现，各自只保留后端特定的读/解码细节。
消除三份重复的 `DecodeLoop` 样板（chunk 边界、seek 语义、退出等待、join 顺序）。

**验收**：三个 source 行为等价（播放中 seek / 停止后 seek 再播 / 播放中切换后端 / 退出不崩溃），
复用既有音频测试 + 新增 producer 生命周期单测（含退出等待与 join 语义）。

### T4：`ThreadRegistry` + `ThreadDebugger`（P3）

**现状**：`JobDebugger` 只展示 JobSystem 的 job（提交/运行/吞吐），**看不到专用线程**
（log writer、音频解码线程），无法看引擎并发全景。

**方案**：
1. 建 `ThreadRegistry`：统一注册线程的 `name / id / 角色`（job worker / 专用生产者 / 主线程）。
   JobSystem 的 worker 与所有专用线程启动时都注册，退出时注销。
2. 建 `ThreadDebugger` 面板：展示引擎全部线程及角色，与 `JobDebugger` 的 job 视图互补。

**验收**：面板能列出所有引擎线程及角色；线程注册/注销不引入生命周期问题（registry 访问线程安全）。

## 4. 里程碑

| 里程碑 | 内容 | 验收标准 | 状态 |
|---|---|---|---|
| M1 | Packager 哈希/压缩 fork-join（T1） | 并行 == 顺序逐字节一致 + 顺序回退 | 未开始 |
| M2 | 音频 `DecodeProducer` 抽象（T3） | 三 source 行为等价 + producer 单测 | 未开始 |
| M3 | 物理宽/窄相并行（T2，门槛触发） | 确定性一致 + profiling 实测提升 | 未开始（门槛） |
| M4 | ThreadRegistry + ThreadDebugger（T4） | 并发全景面板 + 线程安全注册 | 未开始 |

## 5. 风险与边界

1. **顺序写依赖是硬约束**（Packager `data_offset`、求解器顺序冲量）：可并行的一定是「无共享可变状态、
   结果按序回填」的纯计算；凡有跨单元写依赖的环节（落盘、求解归并）一律串行。
2. **并行是手段不是目的**：每个候选都设验收/门槛，未达门槛（如物理）不做，避免为了"并行"引入
   同步开销与维护负担。
3. **专用线程 ≠ job**：长生命周期生产者保持专用线程，只做抽象统一，不改其性质；JobSystem 仍是
   引擎短生命周期 job 的唯一真源。

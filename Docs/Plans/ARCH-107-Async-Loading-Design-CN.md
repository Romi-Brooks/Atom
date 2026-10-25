# ARCH-107 异步资源加载设计（接口已冻结，实现已落地）

> 状态：**已实现**。接口契约已冻结（`Future`/`Promise` + `ResourceManager::AcquireAsync`），
> P1/P2/P4 已落地并通过契约测试；P3 的 MusicCard 迁移经评估后**部分落地**（背景纹理已
> 异步化，封面/音频手写状态机判定为不迁移，见"阶段划分"与"退役路径"）。
>
> 关联落地文件：
> - `Concurrency/Future.hpp` —— `atom::async::{Future, Promise}`（纯头文件）。
> - `Asset/ResourceManager.hpp` —— `AcquireAsync`/`AcquireAsyncBatch`/`PumpAsync` 声明 +
>   `AsyncLoad` 内部结构；`AcquireAsync` 模板已实现。
> - `Asset/ResourceManager.cpp` —— `StartAsyncLoad`/`CancelAsyncLoad`/`PumpAsync` 实现。
> - `Error/Error.hpp` —— `ErrorCode::FutureCancelled`（Concurrency 0x0704）。
> - `Render/Resources/TextureCache.{hpp,cpp}` —— `LoadAsync` 已重构到 `Future<TextureHandle>`
>   + 渲染线程 `PumpAsync()`；手写 `TextureLoadState`/`TextureLoadRequest` 已退役。
> - `Render/Text/FontFileLoader.{hpp,cpp}` —— 字体字节 loader（`TypedResourceLoader<FontData>`，
>   `AssetKind::Font`），字体字节进入异步 acquire 机制。
> - `Example/MusicCard/MusicCardScreen.{hpp,cpp}` —— 背景纹理改为 `LoadAsync` + 每帧
>   `PumpAsync()`，不再在渲染线程同步解码。
> - 测试：`Test/Asset/ResourceManagerAsyncTest.cpp`、`Test/Asset/FontFileLoaderTest.cpp`、
>   `Test/Render/TextureCacheTest.cpp`。

## 目标（一句话）

给引擎一个**通用异步加载设施**：调用方在主线程写一行 `AcquireAsync` 立即拿到
`Future`，加载（I/O + 解码）在 job worker 上跑，结果在**帧边界回渲染线程**终结入
缓存，调用方每帧 poll `IsReady()` 即可——不再让每个消费者手写一套
"Schedule + 世代 + 锁 + 回传" 状态机。

## 为什么现在需要它（现状）

仓库里已有多处**手写的 async 状态机**，各自重复同一套逻辑：

| 手写实现 | 位置 | 重复了什么 |
|---|---|---|
| 封面异步解码 | `MusicCardScreen::DecodeTrackArtwork` | Schedule + generation + revision + 锁 + 回写 |
| 音频异步加载 | `MusicCardScreen::LoadTrackAudio` | 同上（另一个变体） |
| 批量图片解码 | `ImageBatchDecoder::DecodeBatch` | 并行 job + WaitAll（阻塞） |
| 单张纹理解码 | `TextureCache::LoadAsync`（本次新加） | 手写 `TextureLoadRequest`（一个窄版 future-like） |

这些都应建立在同一个 primitive 上，而不是各自演化。

## 核心决策

1. **Pull，不 push**。渲染线程每帧 poll `Future::IsReady()`；不做"完成回调队列推到
   渲染线程"。理由：与现有代码一致（MusicCard 每帧检查 flag），且不需要新增一套
   帧边界回调队列。
2. **`Promise`（生产者）/ `Future`（消费者）分离**，共享一个 one-shot 状态盒。
   worker 侧持 `Promise`，调用方持 `Future`。
3. **终结在渲染线程**。`ResourceManager` 的 `AcquireAsync` 中，worker 只加载**原始
   资源**（`shared_ptr<void>`），**不入缓存**；`PumpAsync()`（渲染线程、每帧一次）
   做单线程的缓存插入 + 类型检查，然后 `SetValue` 完成 Future。缓存永不并发写。
4. **世代 = Cancel**。primitive 只携带 `generation` 标签 + `Cancel()`；"旧加载被新加载
   取代"由上层（`ResourceManager`/调用方）对旧 Future 调 `Cancel()` 表达，worker 在
   终结前检查状态，已取消则丢弃结果、不入缓存。

## 类型契约（`atom::async`）

```cpp
enum class FutureState : uint8_t { Pending, Ready, Failed, Cancelled };

template <typename T> class Future {
    auto IsReady() const -> bool;                 // 非 Pending
    auto State() const -> FutureState;
    auto Progress() const -> float;               // 0..1，仅 Pending 期有意义
    auto ProgressMessage() const -> std::string_view;
    auto Generation() const -> uint64_t;
    auto TryGet() const -> const Result<T>*;      // 非阻塞；Pending 返回 nullptr
    auto Get() -> Result<T>;                      // 阻塞至终结；Cancel→FutureCancelled
    auto Cancel() -> void;                        // Pending→Cancelled，幂等
};

template <typename T> class Promise {
    explicit Promise(uint64_t generation = 0);
    auto GetFuture() const -> Future<T>;
    auto SetProgress(float p, std::string_view msg = {}) -> void;
    auto SetValue(T value) -> void;               // →Ready，幂等（one-shot）
    auto SetError(Error error) -> void;           // →Failed，幂等
    auto Cancel() -> void;
};
```

- `T` 约定为**廉价可拷贝**（handle / `shared_ptr`）；`Get()` 按值返回。
- `Future` 可拷贝（`shared_ptr<State>`），对齐 `ResourceHandle`/`JobHandle`；
  `Promise` 唯一生产者、move-only。
- 错误语义：`Cancel()` 或已取消的 `Get()` 返回 `ErrorCode::FutureCancelled`
  （Concurrency 0x0704，已新增）。

## `ResourceManager` 集成（已实现）

```cpp
template <typename T>
auto ResourceManager::AcquireAsync(const ResourceId& id) -> async::Future<ResourceHandle<T>>;
template <typename T>
auto ResourceManager::AcquireAsyncBatch(const std::vector<ResourceId>& ids)
    -> std::vector<async::Future<ResourceHandle<T>>>;
auto ResourceManager::PumpAsync() -> void;   // 渲染线程，每帧一次
```

时序：

```text
调用线程（主/渲染）        worker job                    渲染线程（下一帧）
  AcquireAsync(id)  ──►   loader->Load(fs,id)
      │ 立即返回 Future         │ 写原始 shared_ptr<void> + Error
      ▼                         ▼                         PumpAsync()
  每帧 poll IsReady()      （不入缓存）              ──► 取原始结果 → InsertRecord
      │                                                 → 类型检查 → SetValue / SetError
      ▼                                                 ▼
  Future::Get() ──► 返回 ResourceHandle<T>（或失败原因）
```

- 快路径：已缓存 → 直接返回一个已 Ready 的 Future。
- 调度器未启动 → 退化为同步加载后立即 Ready（对齐 `AcquireBatch` 的 fallback）。
- 重复 id 在 batch 内共享一次加载；`Evict` 使在途 Future 被 Cancel。

## 已接入的 consumer

- **字体字节**（`FontFileLoader`，`DecodedImageLoader` 的姊妹 loader，`AssetKind::Font`）：
  让字体字节进入 `AcquireAsync`，落实 README "渲染线程首次遇到 CJK 不得同步加载字体"；
  为 W4 的"字体异步加载 + atlas LRU"供料。**注意**：它只读字节，不建 `FT_Face`——face
  创建属渲染线程（或 W2 的 per-worker）职责。
- **`TextureCache::LoadAsync`**：已重构到 `Future<TextureHandle>`，去掉手写
  `TextureLoadState`/`TextureLoadRequest`；worker 解码、渲染线程 `PumpAsync()` 终结。
- **MusicCard 背景纹理**：`EnsureBackgroundTexture` 从同步 `AcquireFromFilesystem` 改为
  `LoadAsync` + 每帧 `PumpAsync()`，渲染线程不再同步 stb 解码壁纸。

## 与现有代码的关系 / 退役路径

- ✅ `TextureCache::LoadAsync` 的 `TextureLoadRequest` 已退役，改由 `Future<TextureHandle>`
  表达；`LoadAsync` 便捷签名保留（返回类型换成 `Future`）。
- ⚠️ `MusicCardScreen::DecodeTrackArtwork` / `LoadTrackAudio` 经评估**不迁移**，理由：
  - `LoadTrackAudio` 是 `MusicPlayer::Load` 的 streaming 副作用（MusicPlayer 有自己的
    track registry + id 空间 + 生命周期），不是"加载资源返回 handle 入缓存"，套
    `AcquireAsync` 会引入双 id 空间与不必要的 loader 抽象。
  - `DecodeTrackArtwork` 的产物是"缩略图 + 配色"（`MakeCoverThumbnail` /
    `BuildMusicCardTheme` 展示策略副作用），且 `artwork_revision`/`cache_generation_`
    失效检测仍是必需的——Future 只替换"完成信号"，不消除失效检测，收益为负。
  - 两处手写逻辑已被正确 job 化 + 锁保护，务实判断是保留现状，而非为"统一"牺牲清晰度。
  - 真正符合 `AcquireAsync` 语义的 MusicCard 点（背景壁纸）已迁到 `LoadAsync`（见上）。

## 阶段划分

### P1（已完成）——地基 + 通用异步 acquire 闭环

**范围**：`Future/Promise` one-shot pull primitive + `ResourceManager::AcquireAsync` /
`PumpAsync`，契约测试锁住行为。最小可用闭环：worker 解码 → 渲染线程终结入缓存 →
调用方每帧 poll。

### P2（已完成）——`TextureCache::LoadAsync` 消费 `Future`

手写 `TextureLoadState`/`TextureLoadRequest` 退役，`LoadAsync` 改返 `Future<TextureHandle>`
+ 渲染线程 `PumpAsync()`；快路径/共享在途/错误空 handle 语义与原同步 acquire 对齐。

### P3（部分完成）——MusicCard 迁移评估

- ✅ 背景壁纸：同步解码 → `LoadAsync` + `PumpAsync`（消除渲染线程 stb 解码）。
- ⛔ 封面/音频手写状态机：判定为不迁移（理由见"退役路径"），保留现状。

### P4（已完成）——字体字节 loader

`FontFileLoader`（`TypedResourceLoader<FontData>`，`AssetKind::Font`）接 `AcquireAsync`；
字体字节异步读取，`FT_Face` 创建留渲染线程/W2。为 W4 的 atlas LRU 供料。

### P5（未做）——批量异步 + 进度

`AcquireAsyncBatch` 的进度聚合；大文件按块进度由各解码器自行上报（不进 primitive），
此处只做聚合透传。暂缓，按需推进。

### 明确不做（所有阶段都不进 primitive）

- 不做 push 式完成回调队列（render-thread drain list）——pull 已够用。
- 不做流式/分块进度（音频大文件按块报进度属解码器职责，不进 primitive）。
- 不做取消传播链（`Cancel` 只终结自身 Future，不递归取消其依赖 job）。
- 不做 `Future::Then`/组合子（.then/.map）——需要时再以 job 依赖表达。

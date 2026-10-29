# Atom 子系统设计与实施记录

> 状态：**合并文档**（2026-10-29）。原先 `Docs/Plans` 下的 8 份子系统文档按主题并入本文，原文内容逐字保留。
> 任务排期与优先级见 `TaskList-CN.md`，缺陷与技术债见 `../Remaining-Issues.md`；
> 本文件只回答「某个子系统当初为什么这样设计、现在做到哪一步」。

| 节 | 主题 | 现状 |
|---|---|---|
| A | 资源系统 / VFS / APKG / 异步加载 | 主体已实现；APKG v2 第一阶段在 `feat-packager` 分支 |
| B | 并发与 JobSystem | 三站接入已完成；剩余候选见本节 |
| C | 物理 | M1–M6 与 M6.5（sensor/user_data/persist）已实现；M7（Box2D）可选 |
| D | 时间系统 | P1/P2 已实现；P3（音频设备钟）/P4（渲染插值）待做，P4 已由 T1.4 部分落地 |
| E | Shader 分层 | 设计完成，待实现 |
| F | Lua Mod | 设计完成，待实现 |
| G | 2D 核心范围 | **已冻结**（P0 范围定义） |

---

## A. 资源系统、VFS 与异步加载（ARCH-107）

<!-- 来源：Subsystem-Plans-CN.md -->
## ATOM 资源系统与 VFS 设计及实施计划（ARCH-107）

> 状态：**阶段 A、B、C1、C2、C3、D 已落地**；APKG v2 第一阶段（store-only + 单 chunk 写端）已落地于 `feat-packager` 分支。
> 关联：`Remaining-Issues.md` 的 ARCH-107。
> 建立日期：2026-09-12
> 进度更新：2026-09-29（合并原 `Filesystem-Design-CN.md` 与 `Subsystem-Plans-CN.md`）

本文由原两份文档合并而成：前半部分（§1–§4）是**设计语义**（路径模型、模块边界、挂载规则、APKG 演进），
后半部分（§5 起）是**实施进度**（阶段划分、决策、量级估计）。设计决定"是什么"，计划记录"做到哪、还差什么"。

---

### 1. 目标与非目标

ATOM 需要拥有自己的文件系统语义，而不是把 `std::filesystem` 的类型传播到业务、渲染和资源模块。运行时资源必须能同时来自开发目录、内存、正式资源包和未来的网络/模组挂载点。

第一阶段的目标是稳定公共边界：UTF-8 虚拟资源路径、确定的错误码、随机读取、目录枚举与挂载后端接口。`std::filesystem` 可以暂时留在 Native 后端 `.cpp` 中；它不是公共 API，也不是资源身份。

这不是重写 STL 容器的项目。`std::vector`、`std::string` 等可以作为实现细节和普通值容器继续使用；ATOM 自己必须定义的是路径规则、挂载优先级、I/O 行为、安全边界和资源格式。

### 2. 路径模型

运行时仅使用 `AssetPath`：

```text
res://textures/ui/button.png
engine://shaders/spirv/sprite.spv
```

- 全部 UTF-8，路径分隔符固定为 `/`；
- 格式为 `mount://relative/path`，mount 为小写 ASCII；
- 禁止反斜杠、空 segment、`.`、`..`、盘符和绝对宿主路径；
- `res://` 是一个合法挂载根，可用于目录枚举；
- 真实磁盘路径只能留在 Native backend 和工具链中，不能进入渲染、音频或资源加载器的公开接口。

这使同一个资源在开发时映射到目录、发布时映射到 APKG，而调用点保持不变。

### 3. 模块边界

```text
Atom_UTF8             UTF-8 校验与平台编码转换
     │
Atom_FS               AssetPath, IFile, IFileSystem, Vfs
 ├── NativeFileSystem  本地目录，根目录/符号链接越界保护
 ├── MemoryFileSystem  测试与动态生成资源
 ├── OverlayFileSystem 按优先级覆盖多个挂载
 └── PackageFileSystem APKG v1（只读；v2 未实现）
     │
Atom_Assets           ResourceId, LoaderRegistry, ResourceHandle<T>, cache
     ├── DecodedImageLoader（VFS → DecodedImage，已接入）
     ├── ScriptSourceLoader（VFS → 脚本源码，已接入）
     ├── Audio loader（已接入：AudioClipResourceLoader + AudioClipLoader VFS 入口）
     ├── Shader loader（未接入）
     └── Model/video/font loader（未接入）
```

`IFile` 提供 `Size()`、随机 `ReadAt()` 和顺序游标（`Tell`/`Seek`/`ReadNext`）。纹理与小配置可以一次读取；音频、视频和大模型可以按块读取。每个 `IFile` 只允许单调用者并发模型，异步 I/O 将在此同步契约稳定后作为独立扩展实现。

Native 后端解析路径后必须 canonicalize，并验证目标仍在挂载根目录下。因此包内路径和通过符号链接间接到达的文件都不能越出受信根目录。

### 4. VFS 挂载规则

`Vfs` 持有按优先级排序的挂载：

```text
res://  priority 200  DevelopmentDirectory   （开发覆盖）
res://  priority 100  game.apkg              （正式基础包）
engine:// priority 100 engine.apkg
```

`Open` 与 `Stat` 从最高优先级开始命中；`List` 合并目录条目，同名项由高优先级覆盖。挂载与卸载当前立即生效；帧边界/热重载安全点待 CORE-001 建立后再收紧。已打开的 `IFile` 保持有效直到调用者释放。

### 5. APKG 演进

#### v1

现有 APKG v1 是末尾文件表的顺序归档。它保留为只读兼容格式，但不再扩展：路径根依赖 CWD、类型只是扩展名、整个资源需要完整读入内存、没有内容校验和可用的压缩实现。

#### v2

APKG v2 采用"固定头 + 数据 chunks + manifest 索引"的设计：

- Header：魔数、字节序、主/次版本、flags、包 UUID、manifest 偏移/大小与 header checksum；
- Chunk 表：每个块的逻辑/存储大小、压缩算法、内容 hash、数据偏移；
- Asset 表：规范路径、父目录、`AssetKind`、tags、逻辑大小、内容 hash、chunk 范围与 cook 元数据；
- DirectoryNode：父节点与 child 范围，支持高效 `List`；
- Kind/Tag/Dependency 索引：Texture、Config、Audio、Model、Shader、Font、Script、Scene、Video、Binary 及其分类查询；
- StringTable：所有路径、tag 与元数据键去重存储。

所有索引使用连续数组和 index，不在磁盘格式中存储指针。按规范路径 hash 查找后必须比较完整路径，避免 hash collision 被误解析。资源数据以 chunk 形式存放，允许局部解压和流式读取。

打包器先扫描并校验全部输入、构建确定的 `PackagePlan`，写入临时文件，校验 manifest 与内容 hash 后原子替换目标文件；不得删除旧包后再开始生成。重复 AssetPath 必须在任何数据写入前报错。

### 6. 资源分类与树

目录树是人类组织维度，`AssetKind` 是运行时语义，tag/variant 是筛选维度。三者必须并存，不能仅凭扩展名推断类型。

```toml
[[asset]]
source = "Assets/UI/Button.png"
path = "res://textures/ui/button.png"
kind = "texture"
tags = ["ui"]

[[asset]]
source = "Assets/Config/default.toml"
path = "res://config/default.toml"
kind = "config"
```

目录规则可以批量产生条目，但最终每个资源必须拥有确定的 `AssetPath`、`AssetKind`、冲突策略和可选 cook 参数。源资源与平台 cook 产物分离；同一 AssetPath 可以有 `windows-dxil`、`linux-spirv` 或纹理质量等 variant。

---

### 7. 实施现状

#### 已完成

##### Atom_FS 第一阶段 + 阶段 A

| 已有 | 位置 | 说明 |
|------|------|------|
| `AssetPath`：UTF-8、`mount://`、拒绝 `..`/反斜杠/盘符/空 segment | `Filesystem/AssetPath.hpp/.cpp` | 有测试 |
| `IFile`：`Size` / `ReadAt` / `Tell` / `Seek` / `ReadNext` | `Filesystem/FileSystem.hpp` | D2 顺序游标已落地 |
| `ReadAll` 辅助函数 | `Filesystem/FileSystem.hpp/.cpp` | 供小资源一次读入 |
| `NativeFileSystem`：`weakly_canonical` + `IsWithinRoot` | `Filesystem/FileSystem.cpp` | 防符号链接越界 |
| `MemoryFileSystem`：写入/建目录/删除 + 打开文件共享字节缓冲 | `Filesystem/MemoryFileSystem.hpp/.cpp` | 覆写不使已打开文件失效 |
| `OverlayFileSystem`：高优先级命中，List 合并覆盖 | `Filesystem/OverlayFileSystem.hpp/.cpp` | |
| `Vfs`：按 mount 名挂载表，多后端按 priority 排序 | `Filesystem/Vfs.hpp/.cpp` | 挂载/卸载立即生效 |
| `PackageFileSystem`：APKG v1 只读挂载 | `Filesystem/PackageFileSystem.hpp/.cpp` | 独立解析，不依赖 `Utilities/Packager`；`List` 前缀合成目录 |
| CMake target `Atom_FS` | `CMakeLists.txt` | 全部新后端已编入 |
| CTest | `Filesystem/Test/` | `AssetPath` / `NativeFileSystem` / `MemoryFileSystem` / `OverlayVfs` / `PackageFileSystem` |

##### Atom_Assets 阶段 B

| 已有 | 位置 | 说明 |
|------|------|------|
| `AssetKind` + 字符串转换 | `Asset/AssetKind.hpp/.cpp` | Texture/Audio/Shader/Config/Script/… |
| `ResourceId`：path + kind + variant，canonical key | `Asset/ResourceId.hpp/.cpp` | variant 允许 `[a-z0-9_-]{0,64}` |
| `ResourceHandle<T>`：shared 句柄 + generation | `Asset/ResourceHandle.hpp` | 缓存 Evict 后旧句柄仍有效 |
| `IResourceLoader` / `TypedResourceLoader<T>` | `Asset/IResourceLoader.hpp/.cpp` | `ResourceType()` 供类型检查；`MapFsResult` |
| `ResourceManager`：按 kind 注册 loader、按 id 去重缓存 | `Asset/ResourceManager.hpp/.cpp` | D5 只去重；D6 同步 |
| `SetRecycleCallback` | 同上 | 最后句柄释放时回调，经共享 `RecycleHooks` 对已缓存记录也生效 |
| CMake target `Atom_Assets` | `CMakeLists.txt` | PUBLIC `Atom_FS`，已挂入 `Atom_Core` |
| CTest | `Asset/Test/ResourceManagerTests.cpp` | `Atom_Assets.ResourceManager` |

##### 阶段 C（消费者迁移）

- **C2 Script — 已完成**：`ScriptSourceLoader`（kind=Script）VFS 读入产出可共享 `std::string`；`LuaLoader::LoadScript/ReloadScript(IFileSystem, AssetPath)` 走 `IFile` + `luaL_loadbuffer`；`LoadScriptSource` 执行已缓存源码。
- **C1 Texture — 已完成**：`DecodedImageLoader`（VFS → `DecodeImageMemory` → `DecodedImage`）+ `LoadTextureFileSystem` + `TextureCache`/`TextureHandle`；GPU 销毁走 `Renderer2D::EnqueueDeferredTextureDestroy` + `FlushDeferredTextureDestroys`（帧外）。
- **C3 Audio — 已完成**：`IAudioDecoder` 新增 `OpenStream(IFile&)` 纯虚并删除旧 `Open(path)`；`RiffWaveReader` 用 `IFile::ReadAt` 流读、`Minimp3Decoder` 用 `IFile` 回调 IO、`SDL3WavDecoder` 全量读回退内存；`AudioClipLoader` 提供 `Load(IFileSystem, AssetPath)` / `OpenStreaming(IFileSystem, AssetPath)`，`StreamingResult` 持有 `IFile` 保证流式解码器生命周期。
- 退出标准：三条链路都不再出现 `std::filesystem` 或裸路径字符串。（Script/C1/C3 已满足生产路径；Shader 仍后端内部路径）

##### 阶段 D：验收与回归

- 可运行验收示例 `Test/Asset/StageDAcceptanceTest.cpp`：从目录挂载（MemoryFileSystem，priority 200）与 APKG 挂载（APKG v1，priority 100）分别用同一 URI 加载 Texture + Audio + Script，并断言共享同一实例；含高优先级覆盖低优先级的回归断言。
- `AudioClipResourceLoader`（kind=Audio）让音频与 Script/Texture 一样走 `ResourceManager` 统一共享。
- `MusicPlayer` 新增 `Load(IFileSystem, AssetPath)`，`Track` 持有 `IFile` 所有权保证流式解码器生命周期。
- CTest `Atom_Assets.StageDAcceptance`。

#### 完全缺失（下一阶段）

- APKG v2（chunk / manifest / StringTable / `PackageBuilder`）

> **2026-10-07 更新（feat-packager 分支）**：APKG v2 第一阶段已落地——格式契约
> `Filesystem/PackageFormatV2.hpp`（固定头 + 数据区 + manifest 段目录，string/chunk/asset/dir 四段）、
> `PackageBuilder`（计划先校验、临时文件 + 回读校验 + 原子替换、确定性输出）、`PackageFileSystem` v2 读取
> （真实目录表 Stat/List、跨 chunk IFile、store-only）。剩余：压缩算法、多 chunk 写端、tags/依赖/cook 段。
> `packager_tool` 已接入 v2：`packager_tool pack2 <input> <out> [package_path]` 非交互子命令。

### 8. 范围边界

**明确不做（留到后续）**：ARCH-107 第 4 条——异步加载、热重载、依赖图、内存预算。
原因：这四项依赖当前不存在的基础设施。

- 异步加载需要 Job System / 线程池，仓库目前只有音频里 3 处 `std::thread`；
- 热重载与挂载变更需要"帧边界安全点"，依赖 CORE-001（固定时间步）的帧语义；
- 内存预算 + LRU 需要与 GPU 延迟释放、正在录制的 draw packet 生命周期协同。

**待定**：APKG v2 是否纳入下一阶段"透明挂载"验收（当前 v1 已可挂载，v2 建议另立任务）。

### 9. 阶段划分与退出标准

#### 阶段 A：补完 `Atom_FS` — **已完成**

- 目标：四个后端全部可用，VFS 能按优先级挂载并透明命中。
- 交付物：MemoryFileSystem、OverlayFileSystem、Vfs、PackageFileSystem（v1 只读）、`IFile` 顺序游标；测试覆盖路径/Unicode/根目录逃逸/目录顺序/重叠挂载优先级/包内读取/顺序读。
- 退出标准：同一 `AssetPath` 在"目录挂载"和"APKG 挂载"下都能 `OpenRead`，高优先级覆盖低优先级，非法路径与损坏包返回受控 `Result`。（已通过 CTest）
- 已知限制：APKG v1 无目录表，`List` 从路径前缀合成；`AssetKind` 仍只能靠扩展名；`Vfs::Mount`/`Unmount` 立即生效，帧边界安全待 CORE-001 收紧；`MemoryFileSystem` 覆写/删除不使已打开 `IFile` 失效。

#### 阶段 B：`Atom_Assets` 资源层 — **已完成**

- 目标：建立资源身份与共享语义。
- 交付物：`ResourceId`（规范路径 + kind + variant）、`ResourceHandle<T>`（shared + generation）、`IResourceLoader`/`TypedResourceLoader<T>`、`ResourceManager`（按 id 去重）、`SetRecycleCallback`。
- 退出标准：同一 URI 两次请求得到同一实例；最后一个句柄释放后进入 recycle 回调或直接销毁。（已通过 CTest）
- 已知限制：无 LRU / 内存预算；`Acquire` 非线程安全；recycle 回调在句柄析构路径同步调用；尚无真实 Texture/Audio/Script loader（仅测试用）。

#### 阶段 C：消费者迁移 — **已完成**

见 §7 阶段 C。

#### 阶段 D：验收与回归 — **已完成**

见 §7 阶段 D。附带修复：`Utilities/Packager` 的全局 `namespace fs = std::filesystem` 别名与 `atom::fs` 冲突，已改名 `native_fs`。

### 10. 待讨论的设计决策

| 编号 | 决策点 | 备选 | 初步倾向 | 落地情况 |
|------|--------|------|----------|----------------|
| D1 | APKG 范围 | 只做 v1 只读挂载 / 同时做 v2 reader + `PackageBuilder` | 只做 v1，v2 另立任务（v2 约 2–3k 行，等于重写 `Utilities/Packager`） | 已按 v1 落地 |
| D2 | `IFile` 是否增加顺序游标 | 保持纯 `ReadAt` / 增加 `ReadNext`+`Tell` | 增加，并让 `NativeReadFile` 记录当前位置、去掉冗余 `seekg`（音频流式性能关键） | 已落地（含 `Seek`） |
| D3 | 音频流式改造方式 | 取消流式、全量读内存 / 解码器接收 `IFile` 流适配器 | 不取消流式。`minimp3` 用 `mp3dec_ex_open_cb` 回调 IO；`RiffWaveReader` 把 `std::FILE*` 换成流接口；`SDL3Wav` 保持内存路径 | 已落地（阶段 C3） |
| D4 | GPU 资源所有权 | 缓存直接销毁 / 句柄计数 + 帧边界延迟回收 | 后者：计数归零只入队，销毁统一在帧边界由渲染线程执行 | 已落地（阶段 C1，`FlushDeferredTextureDestroys`） |
| D5 | 本轮是否做缓存淘汰 | 引入 LRU / 只去重不淘汰 | 只去重不淘汰（淘汰需要内存预算，属于第 4 条） | 阶段 B：只去重，无 LRU |
| D6 | 线程模型 | 本轮就做异步 / 本轮同步，接口预留 | 同步；接口保持"可异步化"（loader 不做线程假设，句柄可跨线程拷贝） | 阶段 B：同步 `Acquire`；句柄可拷贝 |

### 11. 量级估计

| 阶段 | 原估量 | 状态 | 备注 |
|------|--------|------|------|
| A 补完 Atom_FS | 1.2k–1.8k 行 | **已完成** | 实际约 1.5k 行（含测试），5 个 CTest |
| B Atom_Assets | 1.0k–1.5k 行 | **已完成** | 实际约 1.1k 行（含测试），1 个 CTest |
| C 消费者迁移 | 1.0k–1.8k 行 | **已完成** | 原高风险（C3 为主）；已落地 |
| D 验收与回归 | ~0.5k 行 | **已完成** | 验收示例 + CTest + MusicPlayer VFS + AudioClipResourceLoader |

参考：当前 Atom 自有代码约 184 个文件 / 17.7k 行（不含 `ThirdParty/`）。

### 12. 与其它未完成项的关系

- **Job System（缺失）**：异步加载的前置，不在本计划内。
- **CORE-001 固定时间步**：阶段 A 的 `Vfs::Mount`/`Unmount` 与未来 GPU 释放目前依赖调用方在帧安全点调用；若主循环语义建立，应把生效时机改为显式 `ApplyPending`。
- **ARCH-108 Entity 拆分**：与资源系统同属地基，二者无直接依赖，但都应在 RENDER-009 之前落地。
- **RENDER-009 正式 RHI**：GPU 资源所有权模型（D4）应与正式 Buffer/Texture 契约一起定，避免先做一套再推翻。若 RENDER-009 先行，阶段 C1 的句柄模型需要对齐。

### 13. 剩余问题速览（供决策）

按建议优先级排列；括号内为复杂度。

1. **(高) APKG v2 + PackageBuilder**：另立任务；建议在 C+D 验收通过后再做（现已通过）。
2. **(延后) 异步 / 热重载 / 依赖图 / 内存预算 / LRU**：依赖 Job System 与 CORE-001。
3. **(低) 帧边界 API 收紧**：CORE-001 建立后，将 `Vfs` 挂载变更与 TextureCache Flush 改为显式帧安全点。
4. **(低) RENDER-009 对齐**：正式 RHI 的 Texture 契约落地后，复查 `TextureCache`/`GpuTextureRecord` 是否需要改绑 device。

<!-- 来源：Subsystem-Plans-CN.md -->
## ARCH-107 异步资源加载设计（接口已冻结，实现已落地）

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

### 目标（一句话）

给引擎一个**通用异步加载设施**：调用方在主线程写一行 `AcquireAsync` 立即拿到
`Future`，加载（I/O + 解码）在 job worker 上跑，结果在**帧边界回渲染线程**终结入
缓存，调用方每帧 poll `IsReady()` 即可——不再让每个消费者手写一套
"Schedule + 世代 + 锁 + 回传" 状态机。

### 为什么现在需要它（现状）

仓库里已有多处**手写的 async 状态机**，各自重复同一套逻辑：

| 手写实现 | 位置 | 重复了什么 |
|---|---|---|
| 封面异步解码 | `MusicCardScreen::DecodeTrackArtwork` | Schedule + generation + revision + 锁 + 回写 |
| 音频异步加载 | `MusicCardScreen::LoadTrackAudio` | 同上（另一个变体） |
| 批量图片解码 | `ImageBatchDecoder::DecodeBatch` | 并行 job + WaitAll（阻塞） |
| 单张纹理解码 | `TextureCache::LoadAsync`（本次新加） | 手写 `TextureLoadRequest`（一个窄版 future-like） |

这些都应建立在同一个 primitive 上，而不是各自演化。

### 核心决策

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

### 类型契约（`atom::async`）

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

### `ResourceManager` 集成（已实现）

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

### 已接入的 consumer

- **字体字节**（`FontFileLoader`，`DecodedImageLoader` 的姊妹 loader，`AssetKind::Font`）：
  让字体字节进入 `AcquireAsync`，落实 README "渲染线程首次遇到 CJK 不得同步加载字体"；
  为 W4 的"字体异步加载 + atlas LRU"供料。**注意**：它只读字节，不建 `FT_Face`——face
  创建属渲染线程（或 W2 的 per-worker）职责。
- **`TextureCache::LoadAsync`**：已重构到 `Future<TextureHandle>`，去掉手写
  `TextureLoadState`/`TextureLoadRequest`；worker 解码、渲染线程 `PumpAsync()` 终结。
- **MusicCard 背景纹理**：`EnsureBackgroundTexture` 从同步 `AcquireFromFilesystem` 改为
  `LoadAsync` + 每帧 `PumpAsync()`，渲染线程不再同步 stb 解码壁纸。

### 与现有代码的关系 / 退役路径

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

### 阶段划分

#### P1（已完成）——地基 + 通用异步 acquire 闭环

**范围**：`Future/Promise` one-shot pull primitive + `ResourceManager::AcquireAsync` /
`PumpAsync`，契约测试锁住行为。最小可用闭环：worker 解码 → 渲染线程终结入缓存 →
调用方每帧 poll。

#### P2（已完成）——`TextureCache::LoadAsync` 消费 `Future`

手写 `TextureLoadState`/`TextureLoadRequest` 退役，`LoadAsync` 改返 `Future<TextureHandle>`
+ 渲染线程 `PumpAsync()`；快路径/共享在途/错误空 handle 语义与原同步 acquire 对齐。

#### P3（部分完成）——MusicCard 迁移评估

- ✅ 背景壁纸：同步解码 → `LoadAsync` + `PumpAsync`（消除渲染线程 stb 解码）。
- ⛔ 封面/音频手写状态机：判定为不迁移（理由见"退役路径"），保留现状。

#### P4（已完成）——字体字节 loader

`FontFileLoader`（`TypedResourceLoader<FontData>`，`AssetKind::Font`）接 `AcquireAsync`；
字体字节异步读取，`FT_Face` 创建留渲染线程/W2。为 W4 的 atlas LRU 供料。

#### P5（未做）——批量异步 + 进度

`AcquireAsyncBatch` 的进度聚合；大文件按块进度由各解码器自行上报（不进 primitive），
此处只做聚合透传。暂缓，按需推进。

#### 明确不做（所有阶段都不进 primitive）

- 不做 push 式完成回调队列（render-thread drain list）——pull 已够用。
- 不做流式/分块进度（音频大文件按块报进度属解码器职责，不进 primitive）。
- 不做取消传播链（`Cancel` 只终结自身 Future，不递归取消其依赖 job）。
- 不做 `Future::Then`/组合子（.then/.map）——需要时再以 job 依赖表达。

---

## B. 并发与 JobSystem 接入

<!-- 来源：Subsystem-Plans-CN.md -->
## Atom 并发收尾与 JobSystem 接入规划（JobSystem Integration Plan）

> 状态：引擎并发接入的后续规划文档。JobSystem 已作为引擎 job 的唯一真源完成三站接入
> （音频元数据批量、图片批量解码、资源批量预加载）；本文件记录剩余的接入候选与并发全景收尾。
> 更新日期：2026-10-22

### 1. 已确立的并发约定（前置）

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

### 2. 决策总览

| 决策点 | 结论 |
|---|---|
| 接入顺序 | Packager 哈希 → 音频 DecodeProducer → 物理并行（门槛触发）→ ThreadDebugger |
| Packager 并行粒度 | 一 asset 一 job（哈希/压缩），串行落盘保 `data_offset` 顺序 |
| 音频解码线程 | 保持专用线程（worker），只**统一抽象**消除三份重复样板，不改性质 |
| 物理并行 | 先 profiling 门槛，宽/窄相可并行，求解器（顺序冲量）保持串行 |
| 并发全景 | 先建 `ThreadRegistry`（job worker + 专用线程统一注册），再建 `ThreadDebugger` |

### 3. 剩余任务分项

#### T1：Packager 文件哈希/压缩 fork-join（P2，难度中等）

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

#### T2：物理宽/窄相并行（P2，门槛触发）

**现状**：宽相（均匀网格，预留 AABB Tree）与窄相（SAT）当前单线程，逐 body / pair 遍历。

**门槛**：需先 profiling 确认宽/窄相在**真实多刚体场景**是热点；否则并行化的同步/分摊开销大于收益，
属于负优化。**未达门槛不做**。

**方案（仅当 profiling 达标）**：宽相收集 pair → 窄相一 pair 一 job 并行求接触流形（`physics:narrow`，
纯函数无共享可变状态）→ 串行按确定性顺序归并进求解器。**求解器（Sequential Impulse）保持串行**，
因为冲量求解存在跨接触的顺序写依赖，并行会破坏确定性。

**验收**：多刚体场景下结果与单线程**确定性一致**（相同 pair 顺序归并），且 profiling 有实测提升。

#### T3：音频流式解码统一 `DecodeProducer` 抽象（P2）

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

#### T4：`ThreadRegistry` + `ThreadDebugger`（P3）

**现状**：`JobDebugger` 只展示 JobSystem 的 job（提交/运行/吞吐），**看不到专用线程**
（log writer、音频解码线程），无法看引擎并发全景。

**方案**：
1. 建 `ThreadRegistry`：统一注册线程的 `name / id / 角色`（job worker / 专用生产者 / 主线程）。
   JobSystem 的 worker 与所有专用线程启动时都注册，退出时注销。
2. 建 `ThreadDebugger` 面板：展示引擎全部线程及角色，与 `JobDebugger` 的 job 视图互补。

**验收**：面板能列出所有引擎线程及角色；线程注册/注销不引入生命周期问题（registry 访问线程安全）。

### 4. 里程碑

| 里程碑 | 内容 | 验收标准 | 状态 |
|---|---|---|---|
| M1 | Packager 哈希/压缩 fork-join（T1） | 并行 == 顺序逐字节一致 + 顺序回退 | 未开始 |
| M2 | 音频 `DecodeProducer` 抽象（T3） | 三 source 行为等价 + producer 单测 | 未开始 |
| M3 | 物理宽/窄相并行（T2，门槛触发） | 确定性一致 + profiling 实测提升 | 未开始（门槛） |
| M4 | ThreadRegistry + ThreadDebugger（T4） | 并发全景面板 + 线程安全注册 | 未开始 |

### 5. 风险与边界

1. **顺序写依赖是硬约束**（Packager `data_offset`、求解器顺序冲量）：可并行的一定是「无共享可变状态、
   结果按序回填」的纯计算；凡有跨单元写依赖的环节（落盘、求解归并）一律串行。
2. **并行是手段不是目的**：每个候选都设验收/门槛，未达门槛（如物理）不做，避免为了"并行"引入
   同步开销与维护负担。
3. **专用线程 ≠ job**：长生命周期生产者保持专用线程，只做抽象统一，不改其性质；JobSystem 仍是
   引擎短生命周期 job 的唯一真源。

---

## C. 物理系统

<!-- 来源：Subsystem-Plans-CN.md -->
## Atom 物理系统设计（Physics Plan）

> 状态：物理系统的唯一架构与落地规划文档。M1~M6（形状/契约/刚体/宽相位/窄相位/接触求解/固定步+事件+休眠/revolute 关节）已实现，核心刚体物理模块完整，后续按需推进 M7（可选 Box2D 适配器）。
> 更新日期：2026-10-21

### 1. 决策总览

| 决策点 | 结论 |
|---|---|
| 自研 vs 引入库 | **契约先行 + 双后端**：基础自研（Internal 后端），高计算量/关节/CCD 引入 Box2D 作为可替换后端 |
| 与 ECS 关系 | **独立 world + 组件外壳**：物理世界自治，ECS 只放轻量 Collider/RigidBody 外壳组件，系统同步两边 transform |
| 维度 | **2D 先行，接口预留 3D**：契约层抽象维度，只实现 2D 后端 |
| 单位制 | **米制（MKS）+ y 向上**；与渲染（像素 + y 向下）由 PPM 换算 |
| 确定性 | fixed dt + 稳定遍历顺序；**预留**确定性能力，不实现定点数 |

#### 为什么是"契约 + 双后端"而不是二选一

1. 引擎已有 Backend + Registry + 热切换的成熟骨架（Audio / Render），物理走同构路径成本最低、风格零违和。
2. ECS 已有「公开头不出现第三方类型」的边界规则——这几乎强制要求物理库只能藏在 backend 的 `.cpp` 内部，后端可替换是唯一能同时满足该规则 + 引库的架构。
3. 物理系统的难点在**约束求解器**与 **CCD**，不在碰撞检测。碰撞检测（凸形状 SAT/GJK）是可教学、可测试的纯函数，适合自研；求解器是数值深水区，适合在需要时引入成熟库。
4. Box2D（Erin Catto，MIT）是 2D 刚体事实标准，自研时参考其 GDC 论文而非闭门造车。

**换库 = 换一个 `IPhysicsBackend` 实现。** 游戏代码只依赖 `atom::physics::IPhysicsWorld` / `IPhysicsBody`，永远 include 不到任何第三方头。Box2D 只是契约的第一个可选实现，不是依赖。

### 2. 分层架构

```
PhysicsSystem（游戏侧唯一入口，尚未实现）
  ├─ Shapes/        形状（圆/凸多边形/线段）+ 材质 + 质量属性   ← M1 已实现
  ├─ Dynamics/      刚体（Static/Kinematic/Dynamic）+ 半隐式积分   ← M2
  ├─ Collision/     宽相位（网格→AABB Tree）+ 窄相位（SAT/GJK）    ← M2/M3
  ├─ Solver/        冲量求解 + 约束/关节 + 位置修正               ← M4/M6
  └─ PhysicsWorld/  固定步 + 查询/射线 + 碰撞事件 + 休眠           ← M5

Backend/（物理的后端层与引擎其余后端同树，对齐音频分层）
  ├─ Contracts/Physics/  IPhysics* 契约 + PhysicsTypes + PhysicsBackendId   ← M1
  └─ Physics/Internal/   自研 Internal 后端（Box2D 作为未来并列后端）
```

### 3. 单位制与坐标约定

- **物理内部**：米（MKS）+ y 向上（重力 `(0, -9.8)`），角度弧度。
- **渲染**：像素 + y 向下（Renderer2D 约定）。
- **换算**：游戏侧通过 `PPM`（pixels per meter，建议 32~100）做一次集中换算：

  ```text
  physics.x = render.x / PPM
  physics.y = -render.y / PPM
  ```

- 换算集中在游戏侧的同步层，物理模块**永不**出现像素或 y 翻转。这样数值健康（米制），且换渲染分辨率不影响物理行为。

### 4. 契约 API 摘要（已实现）

见 `../../Physics/Shapes/Shape.hpp` 与 `Physics/Contracts/`：

- `Shape::MakeCircle/MakePolygon/MakeSegment` → `Result<Shape>`（非法输入不静默截断）
- `ComputeArea/Centroid/Aabb/MassProperties`（纯函数）
- `IPhysicsBody`：类型、transform、速度、力/冲量/力矩
- `IPhysicsWorld`：`CreateBody` / `Step(dt, vel_iters, pos_iters)` / 重力 / `IContactListener`
- `IPhysicsBackend`：`CreateWorld()` / `Name()`

### 5. 里程碑

| 里程碑 | 内容 | 验收标准 | 状态 |
|---|---|---|---|
| M1 | Shapes + 质量属性 + 契约层 | 形状几何/惯性单测通过 | ✅ 已实现 |
| M2 | 刚体积分 + 宽相位（均匀网格，预留 AABB Tree） | 自由落体/抛体确定性回放一致 | ✅ 已实现 |
| M3 | 窄相位 SAT + 接触流形 | 各形状组合相交/分离用例全过 | ✅ 已实现 |
| M4 | Sequential Impulse 接触求解 + 位置修正 | 堆叠稳定、能量不爆炸、迭代数可调 | ✅ 已实现 |
| M5 | PhysicsWorld 固定步 + 事件 + sleeping | 静态堆叠休眠后零耗时 | ✅ 已实现 |
| M6 | revolute 关节 | 用约束架构实现，证明可拓展 | ✅ 已实现 |
| M6.5 | 契约扩展（sensor / user_data / PersistContact） | 触发器只上报不响应；刚体可反查拥有者；持续接触可表达 | ✅ 已实现 (T3.1a) |
| M7（可选） | Box2D 适配器 + 等价性回归 | 同一批测试两端跑通，验证契约够抽象 | |

### 6. 工业化稳定性要点

1. **确定性**：fixed dt + 稳定遍历顺序（ECS 已有此性质）+ 禁平台相关浮点假设。联机/回放都要它，现在定调最便宜。
2. **Sleeping 第一期就设计**：大量静态堆叠若每帧白算，性能崩且求解器"沸腾"。
3. **速度/位置迭代分离**：调稳定性的两个旋钮，`Step` 签名已预留。
4. **凸性/退化校验在工厂层**：`MakePolygon` 拒绝非凸/退化，而不是让求解器去撞 UB。

### 7. 扩展点与预留

- **复合形状（多 fixture）**：`BodyDef` 目前单形状，追加 `std::vector<Shape>` 即可，不改 `IPhysicsWorld`。
- **关节**：约束求解器架构下新增关节 = 加约束类型，不改框架（M6 验证）。
- **角色控制器（character controller）**：kinematic body + 特殊求解，后续里程碑。
- **确定性/定点**：fixed dt 已铺路，需要时在积分层替换数值类型。
- **新后端**：实现 `IPhysicsBackend` + 在 `PhysicsBackendId` 追加枚举。

---

## D. 时间系统（CORE-001）

<!-- 来源：Subsystem-Plans-CN.md -->
## ATOM 时间系统实施计划（CORE-001）

> 关联：`Remaining-Issues.md` 的 CORE-001。本文记录分域时钟架构、已落地的 P1/P2，以及后续 P3/P4。

### 结论摘要

| 问题 | 结论 |
|---|---|
| 是否自研高精度时钟？ | **否**。母钟绑定 Backend 的 `ITimeSource`，SDL3 实现用 `SDL_GetPerformanceCounter`。 |
| Sim 是什么？是否和 Screen/物理耦合？ | Simulation。**不共享一个 Sim 钟**。`game` / `physics` 是独立 DomainClock，可各自 pause/rate/步长。 |
| 音频硬件时钟？ | **采纳**。P3 用声卡采样钟 `SyncTo`（软 PLL），支撑卡点与状态切换。见下文 P3。 |
| 本期范围 | **P1 + P2 已实现**；P3/P4 本文留档，后续跟进。 |

### 1. 时钟源：SDL 绑定 vs 自实现

| 维度 | SDL `ITimeSource`（已选） | 自实现 QPC/chrono |
|---|---|---|
| 精度 | QPC 级（Windows）/ 等价高精度计数 | `steady_clock` 多数平台也是 QPC，但需自证 |
| 跨平台 | SDL 已抽象 Win/macOS/Linux/主机 | 需 `#ifdef` 或依赖 chrono 质量 |
| 与音视频时间戳 | 同一 SDL 时间体系，便于对齐 | 可能与 SDL 内部时钟漂移 |
| 架构一致性 | 符合 `Backend/Contracts` → 领域模块 | Time 直接绑系统 API |
| 可测试性 | `ManualTimeSource` / `SteadyTimeSource` 注入 | 同样可注入，默认源仍是系统钟 |
| 维护成本 | 复用 SDL | 自己维护各平台 counter |

**决策**：`Backend/Contracts/Time/ITimeSource.hpp` 为契约；`Backend/SDL3/Time/SDL3TimeSource` 绑定 `SDL_GetPerformanceCounter`；测试用 `Time/SteadyTimeSource.hpp` 的 `ManualTimeSource`。

### 2. 分域时钟（避免 Sim 耦合）

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

### 3. 帧序（CORE-001）

```text
Event → FixedUpdate × N (game) → Variable Update (render) → Render → Present
         └ physics 在自己的 domain 上跑 0..M 步
```

- `Screen::FixedUpdate(float)` / `RenderWindow::AddFixedUpdateListener`
- `Screen::Update(float)` 仍为可变步（动画、UI）
- 追帧上限 `max_steps_per_advance`（默认 5），超出 backlog 丢弃，防死亡螺旋
- 固定步 `alpha` 供渲染插值（P4 使用）

### 4. API 入口

```cpp
auto& time = atom::time::TimeSystem::GetInstance();
// Initialize 由 RenderWindow 绑定 backend_->TimeSource()
auto* physics = time.FindDomain(atom::time::domain::physics);
for (u32 i = 0; i < physics->LastTick().steps; ++i)
    world.Step(atom::time::ToFloatSeconds(physics->LastTick().fixed_step_ns));
```

### 5. P3 — 音频硬件时钟（待做）

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
- 是否需要独立 `Atom_Media` 音频中间件时钟，还是直接用 `audio` 域。

### 6. P4 — 渲染插值与 shader time（待做）

**目标**：去掉应用侧 `effect_time_ += delta_time`，统一从 `render` 域取时间。

1. Renderer 2D postprocess 的 `time` 改为 `render->Now()`。
2. 固定步对象用 `game`/`physics` 的 `alpha` 做状态插值，再交给 Render。
3. 可选：`render` 域支持 presentation latency offset（预测上屏时刻）。

**待确认**：

- 插值责任放 Screen 还是 ECS/Transform 层。
- 多 fixed-step 域（game 与 physics 不同步）时插值策略。

### 7. 阶段状态

| 阶段 | 内容 | 状态 |
|---|---|---|
| P1 | ITimeSource、MasterClock、DomainClock、TimeSystem | **已完成** |
| P2 | 固定步 accumulator、追帧上限、alpha、帧序接入 RenderWindow | **已完成** |
| P3 | 音频设备钟 SyncTo、卡点、A/V 对齐 | 本文档设计，待实现 |
| P4 | 渲染插值、统一 shader time | 本文档设计，待实现 |

---

## E. Shader 分层与外部编译

<!-- 来源：Subsystem-Plans-CN.md -->
## Shader 分层与外部编译机制设计方案

> 状态：设计文档（待实现）
> 关联：Render/Shader/AtomShaders.cmake / SDLGPU2DPipelineFactory / Renderer2D

### 〇、根本原则：职责边界（首要）

shader 分层与外部编译机制的**根本原因**是游戏侧与引擎侧的职责边界：

> **游戏侧（Example 及游戏开发者项目）绝对不能反向修改引擎侧（Atom）。**

- 引擎侧（`Render/Shader`）提供**通用**能力与**编译工具链函数**，是稳定、可复用的下层。
- 游戏侧只**消费**引擎能力，通过引擎公开的扩展点接入自己的 shader，**不碰**引擎源码
  与 `AtomShaders.cmake`。
- 任何需要游戏侧改动引擎侧才能生效的方案（如往 `AtomShaders.cmake` 里追加 shader、
  在 `SDLGPU2DPipelineFactory` 里硬编码外部 shader 名）都是**错误方向**，一律否决。

下述「可插拔蒙版」「外部编译机制」都服务于这条边界：让游戏侧有自己的 shader 归属地，
并有一条不侵入引擎的接入路径。

### 一、问题

1. 当前 `Render/Shader/Source` 里的三个后处理 shader
   （ChromaticAberration / Glitch / GaussianBlur）在 uniform 里硬编码了
   `uRegion`（归一化区域）+ `uMask`（corner radius / feather），并在 shader 内
   写了整段「圆角矩形 SDF 蒙版」。这是 MusicCard「卡片圆角弹窗」的场景绑定，
   不是纯通用算法。
2. 游戏开发者要引入自己的 shader，目前只能改 `AtomShaders.cmake`（在里面追加
   `atom_compile_shader(...)`），这**绝对不可接受**——引擎的构建脚本不应被使用方
   修改（违反职责边界）。

### 二、方案 B：把圆角蒙版做成「可插拔」

蒙版职责与后处理算法职责分离，各归其位：

#### 2.1 分层原则

| 层 | 归属 | 内容 |
|----|------|------|
| 通用后处理算法 | `Render/Shader/Source` | 纯全屏 screen-space 滤镜（色差/毛刺/高斯模糊），**无任何蒙版概念** |
| MusicCard 专属组合 | `Example/MusicCard/shaders` | 「圆角卡片蒙版 + 后处理」的场景绑定 |

#### 2.2 可插拔蒙版的具体形态

把圆角矩形 SDF 蒙版抽成一个**可复用的 GLSL 片段**（如 `mask_rounded_rect.glsl`，
通过 `#include` 或 GLSL 编译期拼接引入），后处理 shader 通过一个开关
（`uMaskEnabled` / `uMask.z > 0.5`）决定是否应用蒙版：

- 开关关闭 → 纯全屏算法，零蒙版开销（当前已通过 `has_region` 语义链路实现）。
- 开关打开 → 引用 mask 片段，计算逐像素软边缘 alpha。

这样蒙版逻辑只写一份，通用后处理不再内联 MusicCard 的 SDF。

> 注：圆角软边缘是逐像素 alpha，**scissor 矩形硬裁剪替代不了**（scissor 只能做
> 直角裁剪），所以圆角蒙版必须留在 shader 侧；矩形裁剪已由 C++ 侧
> `SDL_SetGPUScissor` 承担。

### 三、外部 shader 编译：不改 AtomShaders.cmake

#### 3.1 现状的三个障碍

1. `atom_compile_shader(source)` 的 `source_path = ${ATOM_SHADER_SOURCE_DIR}/${source}`，
   而 `ATOM_SHADER_SOURCE_DIR` 写死为 `CMAKE_CURRENT_LIST_DIR/Source`，外部 shader
   不在这里就编不了。
2. 函数定义与内置调用混在同一个 `.cmake` 里，`include` 即触发全部内置编译。
3. 运行时加载（`SDLGPU2DPipelineFactory.cpp`）硬编码了内置 shader 名字
   （`LoadSDLGPUShader(..., "GaussianBlur.frag.glsl", ...)`），外部 shader 即使编译
   出来，也没有运行时加载入口。

#### 3.2 目标设计

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

#### 3.3 待解决的问题（需在实现前敲定）

1. **include 语义**：`.cmake` 里 `find_program(... REQUIRED)` 重复 include 会重跑
   find（无害但冗余），需要确认 `ATOM_SHADER_OUTPUT_DIR` 是否要参数化，避免外部
   项目把产物写进引擎的 binary 目录。
2. **运行时加载入口**：`SDLGPU2DPipelineFactory` 目前只加载硬编码的内置 shader。
   外部 shader 要真正用起来，需要一个「按名字/路径加载外部预编译 shader」的公开
   入口（例如 `IRender2DContext` 或 SDLGPU backend 暴露一个 `LoadExternalShader`）。
3. **uniform 布局契约**：外部后处理 shader 若要复用 PostProcess 的
   `set=3, binding=0` 布局，需明确约定 uniform 结构，否则各写各的。

### 四、推荐落地顺序

1. 先做「可插拔蒙版」：抽 `mask_rounded_rect` 片段，通用后处理改为开关引用；
   MusicCard 专属 shader（如需）移到 `Example/MusicCard/shaders`。
2. 再做「外部编译」：把 `atom_compile_shader` 参数化（绝对路径），分离定义与调用。
3. 最后补「外部运行时加载」入口。

### 五、待办

- [ ] 抽取可复用圆角 SDF 蒙版片段，通用后处理去内联化（方案 B）
- [ ] `atom_compile_shader` 支持绝对路径 / 外部 source dir
- [ ] 外部 shader 运行时加载入口设计
- [ ] 一个「游戏开发者引入自定义 shader」的 Example

---

## F. Lua Mod 资源加载

<!-- 来源：Subsystem-Plans-CN.md -->
## Lua Mod 资源加载设计方案

> 状态：设计文档（待实现）
> 关联：LuaHost / LuaContext / Vfs / AssetPath

### 一、问题

mod 开发者通常只能用 Lua 脚本，无法像游戏开发者那样在 C++ 侧
`NativeFileSystem::Create(...)` + `Vfs::Mount(...)` 挂载磁盘目录。那么 mod 如何
导入自己目录下的资源（音频、纹理、模型、配置）？

### 二、核心结论

**mod 脚本不应、也不能直接挂载目录。** 挂载是「宿主配置」，读取才是「mod
内容逻辑」。二者由不同角色完成：

| 层级 | 角色 | 机制 |
|------|------|------|
| 挂载目录到 VFS | 宿主 C++（游戏开发者） | 读 manifest → `NativeFileSystem::Create` → `Vfs::Mount` |
| 读取资源 | mod Lua 脚本 | 字符串 AssetPath 交给加载器（如 `Atom.Audio.Music.Load(id, "mymod://...")`） |

### 三、为什么不能让 mod 脚本挂载目录

1. **安全越权**：让任意 Lua 脚本 `Mount("/path/to/anything")` 等于给 mod 开
   一张读取宿主任意磁盘文件的通行证，是典型的路径遍历 / 越权漏洞。
2. **类型屏障**：`Vfs::Mount` 需要 `std::shared_ptr<IFileSystem>` 后端，而
   `NativeFileSystem` 是 C++ 类型、私有构造、带 symlink 逃逸防护，Lua 侧无法
   安全表达。
3. **职责错位**：目录挂哪个 mount、根在哪，是宿主配置，不是内容逻辑。

### 四、目标架构：声明式 manifest + 宿主代为挂载

mod 作者**声明自己需要什么**，宿主在加载 mod 时替它执行挂载。

```jsonc
// mod.json（mod 作者提供，放在 mod 根目录）
{
  "name": "my_mod",
  "mounts": [
    { "mount": "mymod", "path": "audio" },
    { "mount": "mymod", "path": "textures" }
  ]
}
```

宿主（游戏开发者的 C++ 侧）加载流程：

1. 定位 mod 根目录（sandboxed，通常是游戏自己的 mod 目录）。
2. 读取 `mod.json`。
3. 对每个 mount：`NativeFileSystem::Create(mount, mod_root + relative, fs)` →
   `Vfs::GetInstance().Mount(mount, priority, fs)`。
4. 再执行 mod 的 `main.lua`。

这样 mod 只能访问自己的目录（`NativeFileSystem` 的 canonicalization 已防 `../`
与 symlink 逃逸），脚本里用 `Atom.Audio.Music.Load("song", "mymod://audio/song.ogg")`
即可，字符串 AssetPath 就是唯一自然边界。

### 五、Lua 侧需要新增的绑定

当前 `Atom.Audio.*` 已支持 `Load(id, "res://...")` 字符串（走 `Vfs::GetInstance()`）。
mod 要读取任意字节流 / 枚举目录，需新增 **`Atom.Filesystem`** 绑定：

| API | 语义 |
|-----|------|
| `Atom.Filesystem.Load(mount_path) -> bytes\|nil` | 读取已挂载资源的原始字节 |
| `Atom.Filesystem.Exists(mount_path) -> bool` | 判断资源是否存在 |
| `Atom.Filesystem.List(mount_dir) -> {{name, type}, ...}` | 枚举目录（`IFileSystem::List` 转 Lua 表） |

#### 安全边界（必须守住）

即使暴露 `Atom.Filesystem.Load`，它也只能访问**宿主已挂载的 mount**，mod 无法
通过它挂新目录，也无法越出已有 mount 的根（`NativeFileSystem` 已有 canonicalization
+ symlink 逃逸防护）。

### 六、可选：宿主侧 `LoadMod()` 帮助函数

为减少宿主样板代码，可提供 `LoadMod(vfs, mod_root)` 封装「读 manifest → 挂载 →
执行 main.lua」的完整流程。这是**引擎提供的便利函数**，不是 mod 的 API。

### 七、待办

- [ ] 新增 `Atom.Filesystem` 绑定（Load / Exists / List）
- [ ] 宿主侧 `LoadMod()` 帮助函数 + manifest 解析
- [ ] Example：一个最小 mod（mod.json + main.lua + 一张纹理/一段音频）演示完整链路

---

## G. 2D 核心范围与地图路线

<!-- 来源：Subsystem-Plans-CN.md -->
## Atom 2D Core Scope v1

> 状态：已冻结 P0 范围；以 Windows/Linux 桌面端的可测试游戏世界底座为目标。
>
> 本文不定义任何具体游戏玩法。游戏项目决定经营、RPG、横版、动作或解谜等规则；Atom 提供可组合的 2D 世界能力。

### 目标与非目标

P0 支持 Windows 与 Linux、键盘鼠标、SDL3/SDL_GPU 后端，以及由一个 `GameWorld` 驱动的小型可玩场景。资源使用 VFS URI（例如 `res://`），游戏逻辑不得依赖平台路径或固定像素尺寸。

P0 不实现手柄、联网、编辑器、无缝大世界流送、动画、粒子、Tilemap、导航、存档或任意脚本对象序列化。它们必须建立在已验证的世界生命周期上。

### 职责边界

```text
Screen       菜单、暂停、加载等呈现导航
GameWorld    一个运行中的 ECS world、固定顺序系统、命令和事件
Scene        一个可序列化、可加载的世界片段（下一阶段）
Prefab       可复用实体模板（Scene 之后）
WorldSession 多个 Scene/WorldChunk 的生命周期与流送（大世界阶段）
```

`Screen` 不保存可序列化游戏实体；游戏 `Screen` 可以拥有或展示一个 `GameWorld`。`Scene` 也不等于开放世界：它既可表示单个小关卡，也可在后续作为 `WorldChunk` 的内容。

### P0 帧与提交顺序

```text
Input -> commit -> FixedUpdate × N -> commit -> Update -> commit -> LateUpdate -> commit
```

系统按注册顺序执行。任何创建、销毁和组件结构变化都通过 `EntityCommandBuffer` 在阶段边界提交；阶段内不得改变 ECS 结构。游戏事件以 FIFO 方式收集，由消费该事件的系统在其所属阶段显式 drain，Runtime 不隐式调用游戏回调。

### 后续地图路线

1. `Scene`：版本化 schema、`res://` 资源 URI、临时 world 加载后原子切换。
2. Tilemap：正交 tile 层、对象层、图块碰撞；对象层保留自由摆放实体能力。
3. `WorldManifest`、`WorldSession` 与 `WorldChunk`：先同步切块，再增加异步流送、预算和跨 chunk 持久状态。

这条路线优先覆盖俯视角的大世界表达，同时不把横版游戏排除在外。单向平台、coyote time、种田、背包、NPC 日程等属于具体游戏扩展，而非 P0 引擎规则。


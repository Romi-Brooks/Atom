# ATOM 资源系统与 VFS 设计及实施计划（ARCH-107）

> 状态：**阶段 A、B、C1、C2、C3、D 已落地**；APKG v2 第一阶段（store-only + 单 chunk 写端）已落地于 `feat-packager` 分支。
> 关联：`Remaining-Issues.md` 的 ARCH-107。
> 建立日期：2026-09-12
> 进度更新：2026-09-29（合并原 `Filesystem-Design-CN.md` 与 `Resource-System-Plan-CN.md`）

本文由原两份文档合并而成：前半部分（§1–§4）是**设计语义**（路径模型、模块边界、挂载规则、APKG 演进），
后半部分（§5 起）是**实施进度**（阶段划分、决策、量级估计）。设计决定"是什么"，计划记录"做到哪、还差什么"。

---

## 1. 目标与非目标

ATOM 需要拥有自己的文件系统语义，而不是把 `std::filesystem` 的类型传播到业务、渲染和资源模块。运行时资源必须能同时来自开发目录、内存、正式资源包和未来的网络/模组挂载点。

第一阶段的目标是稳定公共边界：UTF-8 虚拟资源路径、确定的错误码、随机读取、目录枚举与挂载后端接口。`std::filesystem` 可以暂时留在 Native 后端 `.cpp` 中；它不是公共 API，也不是资源身份。

这不是重写 STL 容器的项目。`std::vector`、`std::string` 等可以作为实现细节和普通值容器继续使用；ATOM 自己必须定义的是路径规则、挂载优先级、I/O 行为、安全边界和资源格式。

## 2. 路径模型

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

## 3. 模块边界

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

## 4. VFS 挂载规则

`Vfs` 持有按优先级排序的挂载：

```text
res://  priority 200  DevelopmentDirectory   （开发覆盖）
res://  priority 100  game.apkg              （正式基础包）
engine:// priority 100 engine.apkg
```

`Open` 与 `Stat` 从最高优先级开始命中；`List` 合并目录条目，同名项由高优先级覆盖。挂载与卸载当前立即生效；帧边界/热重载安全点待 CORE-001 建立后再收紧。已打开的 `IFile` 保持有效直到调用者释放。

## 5. APKG 演进

### v1

现有 APKG v1 是末尾文件表的顺序归档。它保留为只读兼容格式，但不再扩展：路径根依赖 CWD、类型只是扩展名、整个资源需要完整读入内存、没有内容校验和可用的压缩实现。

### v2

APKG v2 采用"固定头 + 数据 chunks + manifest 索引"的设计：

- Header：魔数、字节序、主/次版本、flags、包 UUID、manifest 偏移/大小与 header checksum；
- Chunk 表：每个块的逻辑/存储大小、压缩算法、内容 hash、数据偏移；
- Asset 表：规范路径、父目录、`AssetKind`、tags、逻辑大小、内容 hash、chunk 范围与 cook 元数据；
- DirectoryNode：父节点与 child 范围，支持高效 `List`；
- Kind/Tag/Dependency 索引：Texture、Config、Audio、Model、Shader、Font、Script、Scene、Video、Binary 及其分类查询；
- StringTable：所有路径、tag 与元数据键去重存储。

所有索引使用连续数组和 index，不在磁盘格式中存储指针。按规范路径 hash 查找后必须比较完整路径，避免 hash collision 被误解析。资源数据以 chunk 形式存放，允许局部解压和流式读取。

打包器先扫描并校验全部输入、构建确定的 `PackagePlan`，写入临时文件，校验 manifest 与内容 hash 后原子替换目标文件；不得删除旧包后再开始生成。重复 AssetPath 必须在任何数据写入前报错。

## 6. 资源分类与树

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

## 7. 实施现状

### 已完成

#### Atom_FS 第一阶段 + 阶段 A

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

#### Atom_Assets 阶段 B

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

#### 阶段 C（消费者迁移）

- **C2 Script — 已完成**：`ScriptSourceLoader`（kind=Script）VFS 读入产出可共享 `std::string`；`LuaLoader::LoadScript/ReloadScript(IFileSystem, AssetPath)` 走 `IFile` + `luaL_loadbuffer`；`LoadScriptSource` 执行已缓存源码。
- **C1 Texture — 已完成**：`DecodedImageLoader`（VFS → `DecodeImageMemory` → `DecodedImage`）+ `LoadTextureFileSystem` + `TextureCache`/`TextureHandle`；GPU 销毁走 `Renderer2D::EnqueueDeferredTextureDestroy` + `FlushDeferredTextureDestroys`（帧外）。
- **C3 Audio — 已完成**：`IAudioDecoder` 新增 `OpenStream(IFile&)` 纯虚并删除旧 `Open(path)`；`RiffWaveReader` 用 `IFile::ReadAt` 流读、`Minimp3Decoder` 用 `IFile` 回调 IO、`SDL3WavDecoder` 全量读回退内存；`AudioClipLoader` 提供 `Load(IFileSystem, AssetPath)` / `OpenStreaming(IFileSystem, AssetPath)`，`StreamingResult` 持有 `IFile` 保证流式解码器生命周期。
- 退出标准：三条链路都不再出现 `std::filesystem` 或裸路径字符串。（Script/C1/C3 已满足生产路径；Shader 仍后端内部路径）

#### 阶段 D：验收与回归

- 可运行验收示例 `Test/Asset/StageDAcceptanceTest.cpp`：从目录挂载（MemoryFileSystem，priority 200）与 APKG 挂载（APKG v1，priority 100）分别用同一 URI 加载 Texture + Audio + Script，并断言共享同一实例；含高优先级覆盖低优先级的回归断言。
- `AudioClipResourceLoader`（kind=Audio）让音频与 Script/Texture 一样走 `ResourceManager` 统一共享。
- `MusicPlayer` 新增 `Load(IFileSystem, AssetPath)`，`Track` 持有 `IFile` 所有权保证流式解码器生命周期。
- CTest `Atom_Assets.StageDAcceptance`。

### 完全缺失（下一阶段）

- APKG v2（chunk / manifest / StringTable / `PackageBuilder`）

> **2026-10-07 更新（feat-packager 分支）**：APKG v2 第一阶段已落地——格式契约
> `Filesystem/PackageFormatV2.hpp`（固定头 + 数据区 + manifest 段目录，string/chunk/asset/dir 四段）、
> `PackageBuilder`（计划先校验、临时文件 + 回读校验 + 原子替换、确定性输出）、`PackageFileSystem` v2 读取
> （真实目录表 Stat/List、跨 chunk IFile、store-only）。剩余：压缩算法、多 chunk 写端、tags/依赖/cook 段、
> `packager_tool` CLI 接入 v2。

## 8. 范围边界

**明确不做（留到后续）**：ARCH-107 第 4 条——异步加载、热重载、依赖图、内存预算。
原因：这四项依赖当前不存在的基础设施。

- 异步加载需要 Job System / 线程池，仓库目前只有音频里 3 处 `std::thread`；
- 热重载与挂载变更需要"帧边界安全点"，依赖 CORE-001（固定时间步）的帧语义；
- 内存预算 + LRU 需要与 GPU 延迟释放、正在录制的 draw packet 生命周期协同。

**待定**：APKG v2 是否纳入下一阶段"透明挂载"验收（当前 v1 已可挂载，v2 建议另立任务）。

## 9. 阶段划分与退出标准

### 阶段 A：补完 `Atom_FS` — **已完成**

- 目标：四个后端全部可用，VFS 能按优先级挂载并透明命中。
- 交付物：MemoryFileSystem、OverlayFileSystem、Vfs、PackageFileSystem（v1 只读）、`IFile` 顺序游标；测试覆盖路径/Unicode/根目录逃逸/目录顺序/重叠挂载优先级/包内读取/顺序读。
- 退出标准：同一 `AssetPath` 在"目录挂载"和"APKG 挂载"下都能 `OpenRead`，高优先级覆盖低优先级，非法路径与损坏包返回受控 `Result`。（已通过 CTest）
- 已知限制：APKG v1 无目录表，`List` 从路径前缀合成；`AssetKind` 仍只能靠扩展名；`Vfs::Mount`/`Unmount` 立即生效，帧边界安全待 CORE-001 收紧；`MemoryFileSystem` 覆写/删除不使已打开 `IFile` 失效。

### 阶段 B：`Atom_Assets` 资源层 — **已完成**

- 目标：建立资源身份与共享语义。
- 交付物：`ResourceId`（规范路径 + kind + variant）、`ResourceHandle<T>`（shared + generation）、`IResourceLoader`/`TypedResourceLoader<T>`、`ResourceManager`（按 id 去重）、`SetRecycleCallback`。
- 退出标准：同一 URI 两次请求得到同一实例；最后一个句柄释放后进入 recycle 回调或直接销毁。（已通过 CTest）
- 已知限制：无 LRU / 内存预算；`Acquire` 非线程安全；recycle 回调在句柄析构路径同步调用；尚无真实 Texture/Audio/Script loader（仅测试用）。

### 阶段 C：消费者迁移 — **已完成**

见 §7 阶段 C。

### 阶段 D：验收与回归 — **已完成**

见 §7 阶段 D。附带修复：`Utilities/Packager` 的全局 `namespace fs = std::filesystem` 别名与 `atom::fs` 冲突，已改名 `native_fs`。

## 10. 待讨论的设计决策

| 编号 | 决策点 | 备选 | 初步倾向 | 落地情况 |
|------|--------|------|----------|----------------|
| D1 | APKG 范围 | 只做 v1 只读挂载 / 同时做 v2 reader + `PackageBuilder` | 只做 v1，v2 另立任务（v2 约 2–3k 行，等于重写 `Utilities/Packager`） | 已按 v1 落地 |
| D2 | `IFile` 是否增加顺序游标 | 保持纯 `ReadAt` / 增加 `ReadNext`+`Tell` | 增加，并让 `NativeReadFile` 记录当前位置、去掉冗余 `seekg`（音频流式性能关键） | 已落地（含 `Seek`） |
| D3 | 音频流式改造方式 | 取消流式、全量读内存 / 解码器接收 `IFile` 流适配器 | 不取消流式。`minimp3` 用 `mp3dec_ex_open_cb` 回调 IO；`RiffWaveReader` 把 `std::FILE*` 换成流接口；`SDL3Wav` 保持内存路径 | 已落地（阶段 C3） |
| D4 | GPU 资源所有权 | 缓存直接销毁 / 句柄计数 + 帧边界延迟回收 | 后者：计数归零只入队，销毁统一在帧边界由渲染线程执行 | 已落地（阶段 C1，`FlushDeferredTextureDestroys`） |
| D5 | 本轮是否做缓存淘汰 | 引入 LRU / 只去重不淘汰 | 只去重不淘汰（淘汰需要内存预算，属于第 4 条） | 阶段 B：只去重，无 LRU |
| D6 | 线程模型 | 本轮就做异步 / 本轮同步，接口预留 | 同步；接口保持"可异步化"（loader 不做线程假设，句柄可跨线程拷贝） | 阶段 B：同步 `Acquire`；句柄可拷贝 |

## 11. 量级估计

| 阶段 | 原估量 | 状态 | 备注 |
|------|--------|------|------|
| A 补完 Atom_FS | 1.2k–1.8k 行 | **已完成** | 实际约 1.5k 行（含测试），5 个 CTest |
| B Atom_Assets | 1.0k–1.5k 行 | **已完成** | 实际约 1.1k 行（含测试），1 个 CTest |
| C 消费者迁移 | 1.0k–1.8k 行 | **已完成** | 原高风险（C3 为主）；已落地 |
| D 验收与回归 | ~0.5k 行 | **已完成** | 验收示例 + CTest + MusicPlayer VFS + AudioClipResourceLoader |

参考：当前 Atom 自有代码约 184 个文件 / 17.7k 行（不含 `ThirdParty/`）。

## 12. 与其它未完成项的关系

- **Job System（缺失）**：异步加载的前置，不在本计划内。
- **CORE-001 固定时间步**：阶段 A 的 `Vfs::Mount`/`Unmount` 与未来 GPU 释放目前依赖调用方在帧安全点调用；若主循环语义建立，应把生效时机改为显式 `ApplyPending`。
- **ARCH-108 Entity 拆分**：与资源系统同属地基，二者无直接依赖，但都应在 RENDER-009 之前落地。
- **RENDER-009 正式 RHI**：GPU 资源所有权模型（D4）应与正式 Buffer/Texture 契约一起定，避免先做一套再推翻。若 RENDER-009 先行，阶段 C1 的句柄模型需要对齐。

## 13. 剩余问题速览（供决策）

按建议优先级排列；括号内为复杂度。

1. **(高) APKG v2 + PackageBuilder**：另立任务；建议在 C+D 验收通过后再做（现已通过）。
2. **(延后) 异步 / 热重载 / 依赖图 / 内存预算 / LRU**：依赖 Job System 与 CORE-001。
3. **(低) 帧边界 API 收紧**：CORE-001 建立后，将 `Vfs` 挂载变更与 TextureCache Flush 改为显式帧安全点。
4. **(低) RENDER-009 对齐**：正式 RHI 的 Texture 契约落地后，复查 `TextureCache`/`GpuTextureRecord` 是否需要改绑 device。

# ATOM 资源系统与 VFS 实施计划（ARCH-107）

> 状态：**阶段 A、B、C1、C2 已落地**；C3、D 未开工。
> 关联：`Remaining-Issues.md` 的 ARCH-107、`Filesystem-Design-CN.md`（完整设计）。
> 建立日期：2026-09-12
> 进度更新：2026-09-16（C1 TextureCache + 帧边界销毁 + MusicCard）

## 1. 当前状态

### 已完成

#### Atom_FS 第一阶段 + 阶段 A（commit 待填）

| 已有 | 位置 | 说明 |
|------|------|------|
| `AssetPath`：UTF-8、`mount://`、拒绝 `..`/反斜杠/盘符/空 segment | `Filesystem/AssetPath.hpp/.cpp` | 有测试 |
| `IFile`：`Size` / `ReadAt` / `Tell` / `Seek` / `ReadNext` | `Filesystem/FileSystem.hpp` | D2 顺序游标已落地 |
| `ReadAll` 辅助函数 | `Filesystem/FileSystem.hpp/.cpp` | 供小资源一次读入 |
| `NativeFileSystem`：`weakly_canonical` + `IsWithinRoot` | `Filesystem/FileSystem.cpp` | 防符号链接越界 |
| `MemoryFileSystem`：写入/建目录/删除 + 打开文件共享字节缓冲 | `Filesystem/MemoryFileSystem.hpp/.cpp` | 覆写不使已打开文件失效 |
| `OverlayFileSystem`：高优先级命中，List 合并覆盖 | `Filesystem/OverlayFileSystem.hpp/.cpp` | |
| `Vfs`：按 mount 名挂载表，多后端按 priority 排序 | `Filesystem/Vfs.hpp/.cpp` | 挂载/卸载立即生效（见 §6） |
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

#### 阶段 C2 Script + C1 易完成部分

| 已有 | 位置 | 说明 |
|------|------|------|
| `ScriptSourceLoader`：VFS 读入 → 可共享 `std::string` | `Asset/ScriptSourceLoader.hpp/.cpp` | kind=Script |
| `LuaLoader::LoadScript/ReloadScript(IFileSystem, AssetPath)` | `Lua/LuaLoader.hpp/.cpp` | `IFile` + `luaL_loadbuffer`；旧路径 API 已移除 |
| `LuaLoader::LoadScriptSource` | 同上 | 执行已缓存源码，chunk 名用于错误与重载 |
| `DecodedImageLoader`：VFS → `DecodeImageMemory` → `DecodedImage` | `Media/Image/DecodedImageLoader.hpp/.cpp` | kind=Texture，CPU 侧可共享 |
| `LoadTextureFileSystem` | `Render/Resources/ImageTexture.hpp/.cpp` | VFS 读字节再 GPU 上传 |
| `TextureCache` / `TextureHandle` | `Render/Resources/TextureCache.hpp/.cpp` | 按 cache key 去重；最后句柄释放入 deferred 队列 |
| `Renderer2D::EnqueueDeferredTextureDestroy` / `FlushDeferredTextureDestroys` | `Render/Renderer2D/` | 帧外销毁；Shutdown 排空 |
| MusicCard 壁纸/封面 | `Example/MusicCard/MusicCard.cpp` | 壁纸 NativeFileSystem+Vfs；封面 `AcquireFromEncodedMemory`；每帧 Flush |
| CTest | `Asset/Test/ConsumerLoaderTests.cpp`、`Render/Resources/Test/TextureCacheTests.cpp` | `Atom_Assets.ConsumerLoader`、`Atom_Render_Resources.TextureCache` |

### 完全缺失（下一阶段）

- C3 Audio：解码器从路径改为 `IFile` 流
- 阶段 D 验收示例
- APKG v2（chunk / manifest / StringTable / `PackageBuilder`）

### 消费者现状：全部仍是裸磁盘路径

| 资源 | 当前入口 | 问题 |
|------|----------|------|
| Texture | `DecodedImageLoader` / `LoadTextureFileSystem` 已可走 VFS；MusicCard 仍用 `LoadTextureFile`/`DecodeImageFile` | GPU 所有权仍是 renderer 裸指针；MusicCard 未迁 |
| AudioClip | `AudioClipLoader::Load(const std::string&)` / `OpenStreaming(path)` | 三个解码器实现全是路径签名；已有 `OpenStreamingFromMemory` |
| Script | `LuaLoader` 已走 `IFileSystem`+`AssetPath`；`ScriptSourceLoader` 可缓存源码 | 尚无集成验收示例 |
| Shader | `LoadSDLGPUShader(device, const std::filesystem::path& root, ...)` | 后端内部路径，本轮不在验收范围 |

## 2. 范围边界

**下一阶段做**：阶段 C 消费端迁移（Texture / Script / Audio 接入 `IFile` + `ResourceManager`），
以及阶段 D 验收标准（Texture、AudioClip、Script 通过统一 URI 加载并共享资源）。

**明确不做（留到后续）**：ARCH-107 第 4 条——异步加载、热重载、依赖图、内存预算。
原因：这四项依赖当前不存在的基础设施。

- 异步加载需要 Job System / 线程池，仓库目前只有音频里 3 处 `std::thread`；
- 热重载与挂载变更需要"帧边界安全点"，依赖 CORE-001（固定时间步）尚未建立的帧语义；
- 内存预算 + LRU 需要与 GPU 延迟释放、正在录制的 draw packet 生命周期协同（ARCH-107 第 4 条本身就写了"后续"）。

**待定**：APKG v2 是否纳入下一阶段"透明挂载"验收（当前 v1 已可挂载，v2 建议另立任务）。

## 3. 阶段划分

### 阶段 A：补完 `Atom_FS` — **已完成**

- **目标**：四个后端全部可用，VFS 能按优先级挂载并透明命中。
- **交付物**（已落地）
  - `MemoryFileSystem`：测试与动态生成资源
  - `OverlayFileSystem`：按优先级 `Open`/`Stat` 命中，`List` 合并同名覆盖
  - `Vfs`：挂载表 + 挂载/卸载
  - `PackageFileSystem`：APKG v1 只读挂载
  - `IFile` 顺序游标（D2）
  - 测试：路径与 Unicode、根目录逃逸、目录顺序、重叠挂载优先级、包内读取、顺序读
- **退出标准**：同一 `AssetPath` 在"目录挂载"和"APKG 挂载"下都能 `OpenRead`，
  高优先级挂载覆盖低优先级，非法路径与损坏包返回受控 `Result`。（已通过 CTest 验证）
- **已知限制**：
  - APKG v1 没有目录表，`List` 从路径前缀合成；`AssetKind` 仍只能靠扩展名。
  - `Vfs::Mount`/`Unmount` 立即生效；帧边界安全由调用方约定，待 CORE-001 收紧。
  - `MemoryFileSystem` 覆写/删除不会使已打开 `IFile` 失效（共享字节缓冲）。

### 阶段 B：`Atom_Assets` 资源层 — **已完成**

- **目标**：建立资源身份与共享语义。
- **交付物**（已落地）
  - `ResourceId`：规范路径 + `AssetKind` + variant 的稳定身份，canonical key 用于缓存
  - `ResourceHandle<T>`：shared 句柄；`Evict` 后旧句柄仍钉住原实例；带 generation
  - `IResourceLoader` / `TypedResourceLoader<T>`：按 kind 注册，输入走 `IFileSystem` + `ResourceId`
  - `ResourceManager`：按 id 去重；同 URI 同实例；类型不符返回空句柄
  - `SetRecycleCallback`：最后句柄释放时回调（C1 可把 GPU 资源入帧边界队列）
- **退出标准**：同一 URI 两次请求得到同一实例；最后一个句柄释放后进入 recycle 回调或直接销毁。（已通过 CTest 验证）
- **已知限制**：
  - 无 LRU / 内存预算（D5）；`Acquire` 非线程安全（D6）。
  - recycle 回调在句柄析构路径同步调用，尚未与渲染帧边界绑定。
  - 尚无真实 Texture/Audio/Script loader，仅有测试用 `std::string` / `int` loader。
- **依赖**：阶段 A（已完成）。

### 阶段 C：消费者迁移

- **C2 Script — 已完成**：`LuaLoader` 走 `IFile` + `luaL_loadbuffer`；`ScriptSourceLoader` 提供可共享源码资源。
- **C1 Texture — 已完成**：`DecodedImageLoader` + `TextureCache`/`TextureHandle` + 帧边界 `FlushDeferredTextureDestroys`；MusicCard 已迁移。
- **C3 Audio — 未开始**：解码器从"路径"改为"流"。方案见第 4 节 D3。
- **退出标准**：三条链路都不再出现 `std::filesystem` 或裸路径字符串。（Script/C1 已满足生产路径；Audio 未动；Shader 仍后端内部路径）

### 阶段 D：验收与回归 — **未开始**

- **交付物**
  - 一个可运行的验收示例：从目录挂载与 APKG 挂载分别用同一 URI 加载 Texture + AudioClip + Script，
    并断言共享同一实例
  - 新增 CTest target，接入现有 `ATOM_BUILD_TESTS`
  - CMake：新增 `Atom_Assets` target，明确 PUBLIC/PRIVATE 传播边界
- **退出标准**：`Docs/Filesystem-Design-CN.md` 的验收标准全部可复现。
- **风险**：低。

## 4. 待讨论的设计决策

| 编号 | 决策点 | 备选 | 初步倾向 | 落地情况 |
|------|--------|------|----------|----------------|
| D1 | APKG 范围 | 只做 v1 只读挂载 / 同时做 v2 reader + `PackageBuilder` | 只做 v1，v2 另立任务（v2 约 2–3k 行，等于重写 `Utilities/Packager`） | 已按 v1 落地 |
| D2 | `IFile` 是否增加顺序游标 | 保持纯 `ReadAt` / 增加 `ReadNext`+`Tell` | 增加，并让 `NativeReadFile` 记录当前位置、去掉冗余 `seekg`（音频流式性能关键） | 已落地（含 `Seek`） |
| D3 | 音频流式改造方式 | 取消流式、全量读内存 / 解码器接收 `IFile` 流适配器 | 不取消流式。`minimp3` 用 `mp3dec_ex_open_cb` 回调 IO；`RiffWaveReader` 把 `std::FILE*` 换成流接口；`SDL3Wav` 保持内存路径 | 未开始（阶段 C3） |
| D4 | GPU 资源所有权 | 缓存直接销毁 / 句柄计数 + 帧边界延迟回收 | 后者：计数归零只入队，销毁统一在帧边界由渲染线程执行 | 未开始（阶段 C1） |
| D5 | 本轮是否做缓存淘汰 | 引入 LRU / 只去重不淘汰 | 只去重不淘汰（淘汰需要内存预算，属于第 4 条） | 阶段 B：只去重，无 LRU |
| D6 | 线程模型 | 本轮就做异步 / 本轮同步，接口预留 | 同步；接口保持"可异步化"（loader 不做线程假设，句柄可跨线程拷贝） | 阶段 B：同步 `Acquire`；句柄可拷贝 |

## 5. 量级估计（粗估，仅供排期参考）

| 阶段 | 原估量 | 状态 | 备注 |
|------|--------|------|------|
| A 补完 Atom_FS | 1.2k–1.8k 行 | **已完成** | 实际约 1.5k 行（含测试），5 个 CTest |
| B Atom_Assets | 1.0k–1.5k 行 | **已完成** | 实际约 1.1k 行（含测试），1 个 CTest |
| C 消费者迁移 | 1.0k–1.8k 行 | 未开始 | 高风险（C3 为主） |
| D 验收与回归 | ~0.5k 行 | 未开始 | |
| 合计（C+D 剩余） | **约 1.5k–2.3k 行** | — | 不含 APKG v2 |

参考：当前 Atom 自有代码约 184 个文件 / 17.7k 行（不含 `ThirdParty/`），阶段 A 已计入。

## 6. 与其它未完成项的关系

- **Job System（缺失）**：异步加载的前置，不在本计划内。
- **CORE-001 固定时间步（未做）**：阶段 A 的 `Vfs::Mount`/`Unmount` 与未来 GPU 释放目前依赖调用方
  在帧安全点调用；若主循环语义建立，应把生效时机改为显式 `ApplyPending`。
- **ARCH-108 Entity 拆分**：与资源系统同属地基，二者无直接依赖，但都应在 RENDER-009 之前落地。
- **RENDER-009 正式 RHI**：GPU 资源所有权模型（D4）应与正式 Buffer/Texture 契约一起定，
  避免先做一套再推翻。若 RENDER-009 先行，阶段 C1 的句柄模型需要对齐。

## 7. 剩余问题速览（供决策）

按建议优先级排列；括号内为复杂度。

1. **(高) C3 Audio**：解码器签名从路径改为流；独立提交、独立验收。
2. **(低–中) 阶段 D 验收示例**：目录 + APKG 挂载下用同一 URI 加载并断言共享（Script/Texture CPU 已可测；Audio 需先做 C3）。
3. **(高) APKG v2 + PackageBuilder**：另立任务；建议在 C 验收通过后再做。
4. **(延后) 异步 / 热重载 / 依赖图 / 内存预算 / LRU**：依赖 Job System 与 CORE-001。
5. **(低) 帧边界 API 收紧**：CORE-001 建立后，将 `Vfs` 挂载变更与 TextureCache Flush 改为显式帧安全点。
6. **(低) RENDER-009 对齐**：正式 RHI 的 Texture 契约落地后，复查 `TextureCache`/`GpuTextureRecord` 是否需要改绑 device。

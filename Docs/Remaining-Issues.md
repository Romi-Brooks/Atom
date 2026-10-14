# Atom 未完成工作统一清单

> 状态：唯一有效的引擎架构整改与后续规划文档（仅记录引擎侧内容，不含 Example / 产品侧问题）。
> 更新日期：2026-10-01

## 1. 使用规则

- `[ ]` 未开始
- `[-]` 处理中
- `[x]` 已完成（保留实施历史，不删除）
- `[!]` Beta 阶段暂缓

**记录边界**：本文件只跟踪引擎核心（模块、后端、契约、生命周期、构建、测试）的问题与规划。
`Example/` 下的示例应用（如 MusicCard）的 bug 与产品侧内容不在此记录，发现后直接修复或在其自身
目录内维护。

**优先级定义**：

| 优先级 | 含义 | 触发条件 |
|---|---|---|
| P0 | 稳定性 / 确定性 / 崩溃 | 悬垂引用、未定义行为、数据竞争、崩溃 |
| P1 | 核心架构与生命周期 | 影响正确性、可维护性、模块边界的结构性问题 |
| P2 | 能力 / 功能 | 缺失的子系统、渲染 / 音频 / 输入等能力 |
| P3 | 工程化 / 产品化 | 测试基线、CI、SDK、许可证、工具链 |

---

## 2. P0：稳定性与确定性问题

### ARCH-004：引擎事件单次分发

- [x] `SDL3Window::PollEvent()` 只做原始事件 hook 与单次转换，`RenderWindow::ProcessEvents()` 负责唯一一次引擎分发。
- [x] 未映射的 SDL 事件不再以 `EventType::None` 发送给 Screen。
- [x] `SDL_EVENT_QUIT` 与 `SDL_EVENT_WINDOW_CLOSE_REQUESTED` 都转换为关闭事件。
- [x] 平台事件在 SDL3 后端转换为规范化 `IEvent`；普通业务与 ImGui 桥接层均不接收原始 SDL 指针。

### ARCH-105：ScreenManager 生命周期安全（裸指针悬垂）

**背景**：`ScreenManager` 用 `Screen*` 裸指针同时承担「所有权句柄」和「栈内引用」两种职责，
`LoadScreen` 会原地替换同名 Screen 并析构旧对象，但 `current_screen_` 与 `screen_stack_` 仍持有旧裸指针，
形成悬垂引用。

- [x] 将 `current_screen_` / `screen_stack_` 的裸指针改为 `ScreenId` + generation handle 或 `std::shared_ptr`（RAII）。
      已落地：注册表、`current_screen_`、`screen_stack_` 统一为 `std::shared_ptr<Screen>`；替换/卸载时先把每个句柄
      重定向到新对象（或移除），再释放旧对象，因此不存在「旧对象已析构、句柄仍指向它」的窗口。
      容器的修改一律发生在生命周期回调之前，回调之后不再使用任何容器迭代器——`OnDeactivate()` 里重入
      `LoadScreen`/`UnloadScreen` 会让注册表 rehash，这正是旧实现悬垂的另一条路径。
- [x] `LoadScreen` 替换同名 Screen 时，若该 Screen 是 current 或在栈上，必须先明确处理（拒绝、出栈或安全替换），禁止静默析构后留下悬垂指针。
      已落地：默认 `ScreenReplacePolicy::Reject` 返回 `NameConflict` 且不触碰既有注册；显式 `Replace` 时先重定向栈槽，
      对 active 先 `OnDeactivate()` 再激活新对象，保证激活/去激活配平。
- [x] `SwitchScreen` / `PushScreen` / `PopScreen` / `LoadScreen` 返回明确结果（成功 / 未注册 / 同名冲突 / 空指针）。
      已落地：统一 `ScreenOpResult`（`Success` / `NotFound` / `NullScreen` / `InvalidName` / `NameConflict` /
      `Unregistered` / `InUse` / `EmptyStack` / `AlreadyVisible`）+ `ToString()`；重复切换/压入当前 Screen 为
      `AlreadyVisible`，栈空弹出为 `EmptyStack`。
- [x] 禁止静默覆盖 active/stack 中的同名 Screen。已落地：新增 `UnloadScreen()`，active 时返回 `InUse`，
      其余情况连同引用它的栈槽一起移除；不变式「每个存活句柄都指向已注册 Screen」由替换/卸载的重定向保证，
      `GetScreenName()` 因此只依赖注册表，不需要兜底分支。
- [x] 回归：`Atom_Window.ScreenManager`（替换 active/栈上 Screen 后无悬垂、旧对象立即析构、卸载/出栈结果可判定，
      以及 `OnDeactivate` 回调内重入注册表时的安全）。
- 验收：替换、卸载或出栈 Screen 后不存在悬空引用，且所有操作结果可判定。

### ARCH-005：Lua 错误处理与空指针保护

- [x] 检查 `lua_tostring()` 返回空指针的情况，完整记录错误字符串。
      已落地：`LuaHost::HandleError` 对非字符串错误对象改用**受保护**的 `luaL_tolstring` 渲染（table / nil /
      userdata 都能得到可读文本，抛错的 `__tostring` 也只产生一次渲染失败而不会 longjmp 出 C 栈）；
      日志同时带 Lua 状态码名称与错误正文。
- [x] 为 Lua 调用增加 stack guard，保证失败后栈平衡。
      已落地：新增 `Lua/LuaGuard.hpp` 的 `LuaStackGuard`（RAII 恢复 `lua_gettop`）；`LoadScriptSource` /
      `ReloadScript` / `CallLuaFunction` 全部在 guard 下运行，`ReloadScript` 的 `package.loaded` 操作额外判表
      （沙箱环境没有 `package` 时不再对 nil 取域）。
- [x] Music/SFX/Mixer/Transition 注入对象为空或失效时返回受控 Lua 错误。
      已落地：`ContextBridge.hpp` 新增 `RequireLuaContext()`，缺少注入上下文或子系统指针为空时统一走
      `luaL_error`；`LuaMusic`/`LuaSFX`/`LuaVolume` 不再解引用空指针。
- [x] 附带修复：只有真正执行成功的脚本才记入 `loaded_scripts_`；`Initialize()` 重复调用不再泄漏第二个 `lua_State`；
      `luaL_tolstring` 返回空指针时 `Lua.Log.*` 仍保持栈平衡。
- [x] 回归：`Atom_Lua.LuaHost`（语法错误 / 运行时错误 / table / nil / 抛错 `__tostring` / 空注入 / 缺上下文 /
      VFS 重载，逐项断言栈平衡）。
- 验收：语法错误、运行时错误、非字符串错误对象和空注入均不崩溃。

### ARCH-006：公共数值参数缺少统一边界契约

- [x] `SetFPS(0)` 明确表示不做软件限帧，计时器不会除零；重新初始化窗口会恢复门面层保存的 FPS 设置。
- [x] 校验圆形半径、音频 offset、voice 上限和潜在大内存参数。
      已落地：新增 `Backend/Contracts/Numeric/NumericContract.hpp` 作为唯一契约来源。`Renderer2D::DrawCircle`
      校验半径与圆心（NaN/Inf 不再产生 NaN 顶点）、`CreateTexture` 用 `IsValidTextureSize` 拒绝溢出与超过 16384
      的尺寸、后处理 `corner_radius`/`feather` 走同一半径契约；`MusicPlayer::Seek` 与全部 SDL3
      `IAudioSource::SetPlayingOffset` 拒绝非有限 offset；`MusicPlayer::LoadFromMemory` 拒绝空/零长缓冲区；
      `VoicePool` 构造钳制 voice 数，`SetVoiceLimit`/`SFXPlayer::SetVoiceLimit` 显式拒绝越界值；
      `DecodeImageMemory` 先用 `stbi_info_from_memory` 预读头部，在 stb 真正分配之前按**解码后总字节数**
      （`max_decoded_image_bytes`，默认 256 MiB）拒绝超限尺寸；这是总量预算而不是单轴上限，因此 1×20000 的
      细长条仍可解码，而 16384×16384（约 1 GiB）会被拒绝（stb 对 BMP/TGA/PSD 等格式只按 int32 乘积设上界，
      20000×20000 仍会被放行并分配约 1.2 GB）。
- [x] 统一音量范围和非法参数处理方式。
      已落地：0–100 统一由 `numeric::SanitizeVolume` 处理——非有限值收敛为静音，越界钳制；`AudioMixer` /
      `MusicPlayer` / `SFXPlayer` / `VoicePool` 与全部 SDL3 `IAudioSource` 实现共用同一函数，不再各自 `std::clamp`
      （`std::clamp(NaN)` 会原样返回 NaN）。`MusicCrossfade` 的淡入淡出时长同样拒绝 NaN/Inf。
- [x] 回归：`Atom_Backend_Contracts.NumericContract`、`Atom_Media.AudioMixer` / `Atom_Media.VoicePool` /
      `Atom_Media.MusicCrossfade` / `Atom_Media.Image.ImageDecoder`。
- 验收：非法输入不会造成崩溃、除零、溢出或异常大分配。

### ARCH-110：日志并发与实时路径

- [x] 日志级别使用原子或同步访问。已落地：`view_log_level_` 改为 `std::atomic<LogLevel>`（relaxed 读），
      新增 `GetViewLogLevel()`，读取不再需要 logger 锁。
- [x] 使用线程安全时间格式化，Channel 避免无意义复制。
      已落地：时间戳改用 `localtime_s` / `localtime_r` + `snprintf`（原 `std::localtime` 返回共享静态缓冲区，
      多线程同时写日志会互相破坏，现额外记录毫秒便于排查并发顺序）；listener 列表以 `shared_ptr<const Listener>`
      保存，`LogOut` 只做引用计数拷贝而不是逐个复制 `std::function`；`LogOut` 的消息参数按值移动，
      通道名/文件/函数名用 `assign()` 一次成形。
- [x] 音频实时路径不得同步写控制台或持有阻塞锁。
      已落地：控制台输出交给专用 writer 线程 + 有界队列（默认 4096 行；满时丢最旧行，队列长度为 0 表示拒绝
      一切，丢弃数计入 `GetDroppedConsoleLineCount()`）。上限计的是**全部待写行**（已排队 + writer 已取走
      正在写的那一行），因此 `GetPendingConsoleLineCount() <= 上限` 恒成立；调低上限同样会裁剪已排队行
      并计入丢弃数。`LogOut` 只在锁内做「快照 listener + 写 ringbuffer」，
      不做任何 I/O，decoder/音频线程不会阻塞在控制台上；`GetPendingConsoleLineCount()` 可观察在途行数，
      `FlushConsole()` 等待「队列空 **且** 在途写完」，供工具与退出路径排空。
- [-] 后续提供 sink、异步队列和 Release 日志裁剪。异步控制台队列已落地；文件落盘 sink（路径约定、轮转、
      分级）与 Release 编译期裁剪仍待做。
- [x] **启动日志丢失（2026-09-25 发现）**：`Log::LogOut` 原来只实时派发给已订阅 listener、不保存历史，
      而 `LogDebugger` 在 `RenderWindow` 创建后才 `Subscribe`，导致资源准备/加载阶段日志在订阅前已被丢弃。
      已落地 **轻量 ringbuffer** 方案：`Log` 单例维护最近 N 条 `LogRecord`（默认 512），`LogOut` 实时派发的同时写
      ringbuffer，`Subscribe` 在锁外按顺序回放给新订阅者（回调内可再次打日志或退订）；
      `SetRecentCapacity()` / `GetRecentRecords()` / `ClearRecentRecords()` 可调可控，置 0 关闭缓冲。
      **文件写入 sink** 仍作为长期方向保留。

### ARCH-117：Renderer2D 纹理句柄在 Shutdown 后悬垂（新增，2026-10-01）

**背景**：`Renderer2D::Shutdown()` 会 `textures_.clear()` 销毁它持有的全部 `Texture` 对象，但
`TextureCache` 发出的 `TextureHandle` 允许比它活得更久（Screen、资源包、缓存都不必在 renderer
拆解前先释放句柄）。此时 `GpuTextureRecord::~GpuTextureRecord()` 仍调用
`renderer->EnqueueDeferredTextureDestroy(texture)`，而后者第一件事就是读 `texture->owner_`
——对已释放内存的解引用（UB）。MusicCard 示例退出时稳定复现 0xC0000005：
`Renderer2D::Shutdown()` 释放 140 个纹理后，紧接着的 `artwork_textures_` / `background_texture_`
析构就在该路径上崩溃（GDB 下因堆布局不同可能不崩，属典型堆布局相关 UB）。

- [x] `Renderer2D` 新增 `TexturePoolState`（共享存活标志）：`Shutdown()` 先置 `alive = false`
      再销毁纹理对象，`Initialize()` 安装全新 state，因此旧世代句柄永远无法回收新世代纹理。
- [x] `GpuTextureRecord` 持有该 state 的 `shared_ptr`，析构时先判 `IsPoolAlive()` 再触碰
      `texture`/`renderer`；`TextureHandle::IsValid()/Get()` 同样先判存活，Shutdown 后
      句柄自动变为「无效且 `Get() == nullptr`」，不再交出悬垂指针。
- [x] 回归：`Atom_Render_Resources.TextureCache` 用 stub `IRenderDevice`/`IRender2DContext`
      覆盖三种顺序（Shutdown 后释放句柄、renderer 先析构、重新 `Initialize()` 后旧句柄仍失效）；
      把判据还原成修复前的写法该用例会失败。
- 约束：句柄的可见性（`IsValid` / `Get`）必须始终以池存活为准，任何绕过它直接暴露
  `Renderer2D::Texture*` 的新 API 都会重新引入同一类悬垂。

---

## 3. P1：核心架构与生命周期

### ARCH-101：可选的运行时组合层

- [!] Beta 阶段不引入强制 `AudioSystem` 或完整 `EngineRuntime`。
- [ ] 后续根据统一初始化、失败回滚、停止请求、headless 测试和服务生命周期的实际需求，评估可选 `ApplicationRunner/EngineRuntime`。
- 约束：不得破坏用户独立组合 Music、SFX、Mixer 和 Transition 的能力。

### ARCH-107：统一资源系统与 VFS

- [x] `Atom_FS` 第一阶段完成：`AssetPath`、只读 `IFile`（含 `Tell`/`Seek`/`ReadNext` 顺序游标）、`IFileSystem`、`NativeFileSystem`、`MemoryFileSystem`、`OverlayFileSystem`、`Vfs` 挂载表、`PackageFileSystem`（APKG v1 只读，`List` 由前缀合成目录）。完整设计见 `Docs/Plans/Resource-System-Plan-CN.md`；CTest：`Atom_FS.AssetPath` / `NativeFileSystem` / `MemoryFileSystem` / `OverlayVfs` / `PackageFileSystem`。
- [x] `Atom_Assets` 资源层（阶段 B）：`AssetKind`、`ResourceId`（path+kind+variant）、`ResourceHandle<T>`、`IResourceLoader`/`TypedResourceLoader`、`ResourceManager` 去重缓存与 `SetRecycleCallback`（最后句柄释放时回调，供 C1 帧边界回收）。CTest：`Atom_Assets.ResourceManager`。
- [x] 阶段 C2 Script：`LuaLoader::LoadScript/ReloadScript(IFileSystem, AssetPath)` 走 `IFile` + `luaL_loadbuffer`；`LoadScriptSource` 支持执行已缓存源码；`ScriptSourceLoader` 产出可共享的 `std::string` 资源。旧裸磁盘路径 API 已移除。
- [x] 阶段 C1 Texture：`DecodedImageLoader` + `LoadTextureFileSystem` + `TextureCache`/`TextureHandle`；GPU 销毁走 `Renderer2D::EnqueueDeferredTextureDestroy` + `FlushDeferredTextureDestroys`（帧外）。
- [x] 阶段 C3 Audio：解码器从路径改为 `IFile` 流。`IAudioDecoder` 新增 `OpenStream(IFile&)` 纯虚并删除旧 `Open(path)`；`RiffWaveReader`/`WavProfDecoder`/`Minimp3Decoder`（回调 IO）/`SDL3WavDecoder`（全量读回退）各自实现；`AudioClipLoader` 新增 `Load(IFileSystem, AssetPath)` 与 `OpenStreaming(IFileSystem, AssetPath)` 入口，`StreamingResult` 持有 `IFile` 所有权保证流式解码器生命周期。同时确立「string 便捷层 + VFS 核心层」双轨：各加载器另提供 `Load(id, "res://...")` 便捷重载，内部 `AssetPath::TryParse` 后经 `Vfs::GetInstance()` 全局单例落地。
- [x] 阶段 D 验收：目录与 APKG 同一 URI 加载 Texture + Script + Audio 并断言共享。`AudioClipResourceLoader`（kind=Audio）让音频与 Script/Texture 一样走 `ResourceManager`；`MusicPlayer` 新增 `Load(IFileSystem, AssetPath)` 且 `Track` 持有 `IFile`。CTest：`Atom_Assets.StageDAcceptance`。
- [x] APKG v2 reader + `PackageBuilder` 第一阶段落地（`feat-packager` 分支）：格式契约 `Filesystem/PackageFormatV2.hpp`
      （固定 64 字节头 + 数据区 + manifest 段目录：string/chunk/asset/dir 四段，FNV-1a-64 哈希，UTF-8 路径校验
      读写共享）；`PackageBuilder` 先校验全量计划（重复 AssetPath 在任何写入前报错）再写临时文件、回读校验
      header/manifest/chunk 哈希后原子替换；相同输入产出字节一致（确定性 UUID 取自 manifest 哈希）。
      `PackageFileSystem` 按 version 分发：v1 走冻结旧路径，v2 用真实目录表支撑 Stat/List，`OpenRead`
      返回跨 chunk 的 IFile（store-only，拒绝未知压缩算法）。CTest：`Atom_Packager.PackageBuilderV2`。
      未做：压缩算法（format 已预留 compression 字段，当前仅 store）、多 chunk 写端（读端已支持跨 chunk）、
      tags/依赖/cook 元数据段（manifest 段目录可前向扩展）。
      已接入：`packager_tool pack2 <input> <output.apkg> [package_path]` 子命令（非交互，Windows 下经
      `GetCommandLineW`/`CommandLineToArgvW` 取宽参转 UTF-8，规避中文窄 argv 的 GBK 破坏；交互菜单仍为 v1）。
      另新增独立 GUI 工具 `packager_gui`（`Utilities/Packager/PackagerGui.cpp`）：复用引擎 DebugPanel overlay +
      SDL_GPU ImGui 后端，左侧浏览 .apkg 目录树（读端新增 `EnumerateAssets` 暴露 path/kind/size/hash，按
      kind 着色分类），右侧选源目录（内置 std::filesystem 导航器）+ 输出路径 + 前缀打包，打包后自动打开。
- [ ] 后续增加异步加载、热重载、依赖图和内存预算（依赖 Job System）。
- 验收：Texture、AudioClip、Script 可通过统一 URI 加载并共享资源（已通过 `Atom_Assets.StageDAcceptance` 验证）。
- 实施阶段划分（A/B/C/D）、待决问题与当前进度见 `Docs/Plans/Resource-System-Plan-CN.md`。

### ARCH-108：Entity 职责拆分（ECS 前置）

- [ ] 分离 Transform、Health、Renderable、Movement/Combat 等数据与系统。
- [ ] 移除 `const Move()` 通过 mutable 修改状态的设计。
- [ ] Entity 数据不直接持有或加载具体渲染后端资源。
- 方向：从简单 component pool 开始，不直接引入复杂 archetype ECS。
- 关联：`Physics/README.md` 已预留 ECS 职责边界（ECS 持有 entity/component 存储，Physics 持有操作其物理组件的 system）。

### ARCH-109：Lua 对象生命周期与隔离

- [x] `LuaContext` 已替代命名空间全局裸指针（g_music/g_sfx/g_mixer/g_transition/g_filesystem 已移除，改为单一注入结构体，空指针表示「子系统未挂载」）。
- [ ] Entity 绑定使用 ID + generation，避免暴露裸指针。
- [ ] 保证重复 Initialize、Shutdown 和热重载安全。
- [ ] 后续增加内存限制和可选标准库沙箱。

### ARCH-112：扩展回调由单槽改为 Listener Registry

- [x] Event、Update、Overlay、Shutdown 使用 token/RAII Connection 注册。（已落地：`RenderWindow` 提供 `Add*Listener` + `ListenerConnection`。）
- [x] Debugger、Profiler、Console 和用户 Overlay 可以并存。（共享 ImGui context 已落地，见 ARCH-113。）
- [x] 注销一个监听器不得清空其他监听器。（已落地：按 id 独立注销。）
- 约束：监听器不得在自身被分发期间注销。

### ARCH-113：多个 ImGui Overlay 需要共享上下文

- [x] 由窗口级 `OverlayManager` 共享一份 ImGui 上下文；Debugger/LogDebugger/未来的 Profiler/Console 只贡献 panel 内容。
- [x] 旧 SDL_Renderer ImGui 适配器已退出 Atom target，SDL_GPU 的 `imgui_impl_sdlgpu3` 已注册并通过单 Debugger 验收。
- [x] `Log` 提供线程安全的 RAII subscription；`LogDebugger` 是 `DebugPanel` 子类，与自定义面板平级 `Attach`，不再经由 `SetLoggerEnabled` 托管。
- [x] 可见性 `SetEnabled` 与生命周期 `Attach/Detach` 分离；调试面板默认槽位由 `Debugger/PanelLayout` 写入，ImGui `IniFilename=nullptr`，不再依赖 imgui.ini。
- [x] 目录：`Debugger/`（DebugPanel、LogDebugger、FontLoader、PanelLayout、Overlay）；`Window/` 仅保留窗口/Screen/OverlayManager。
- 验收：同一窗口可挂多个 ImGui Overlay 而不互相覆盖。

### ARCH-114：Packager 路径与编码加固

- [x] 跨盘打包导致包内文件名为空 → 已修复：`fs::relative` 跨盘返回空路径（libstdc++）或抛异常（MSVC），`GenerateInternalFilename` 现在 fallback 到裸文件名。
- [x] 路径统一走宽字符转换（复用 `Utf8ToWide`/`Utf8FromWide`）：`Atom_UTF8` 现提供 `PathFromUtf8`/`PathToUtf8`。
- [ ] `packager_tool` 交互输入：中文 Windows 控制台 stdin 为 GBK 字节，与文件系统的 UTF-8 语义冲突（实测报 `Illegal byte sequence`）；读入后按 UTF-8 期望或显式转码。

### ARCH-115：ImGui 原生 Multi-Viewport

- [ ] 启用 `ImGuiConfigFlags_ViewportsEnable`，并补齐 SDL3 platform backend 的 viewport 创建、销毁、移动和输入路由。
- [ ] 为 SDL_GPU renderer backend 实现每个 viewport 的 swapchain/render target 和 draw-data 提交。
- [ ] 在主帧之外调用 platform window update/render 生命周期，并验证资源同步、resize、DPI 和 shutdown。
- 说明：ARCH-113 解决同一主窗口内的多个 ImGui window；ARCH-115 解决可拖出为原生 SDL 窗口。

### ARCH-116：Backend Registry 与 Runtime 职责收敛（新增，2026-09-29）

**背景**：plugable backend 架构目前存在职责重叠——`RenderBackendRegistry`（工厂注册表）与
`RenderBackendRuntime`（内置后端注册入口）分工尚可，但 `BackendRuntime`（音频）同时承担了
「注册表 + 活动后端 + generation + listener」四重职责，与渲染侧「Registry / Runtime 分离」的
对称性不一致。

- [ ] 明确音频侧 Registry（工厂/解码器注册）与 Runtime（活动后端、generation、listener）的边界，与渲染侧对称。
- [ ] 统一「上层只消费 Contract + Registry，Runtime 是唯一认识具体后端的层」这一规则，写进 CODING_STANDARD。
- [ ] 后端切换协议（先建后换 + Quiesce + 原子替换 + generation）是否适用于渲染后端，需在 RENDER-006 前给出结论。

### ARCH-118：MusicPlayer 轨道生命周期（新增，2026-10-01）

**背景**：`Load()` 只为「已加载」这一侧建模，缺少对应的释放口：`tracks_` 只增不减，一旦加载，
decoder、打开的文件句柄和流式 source 的缓冲就常驻到 `Reset()` 或析构。遍历曲库的播放器
（MusicCard 示例）会因此线性增长——实测 154 首后工作集 1051 MB 且持续上升。放大倍率的另一半在
后端：`SDL3StreamingMusicSource` 在构造函数里**无条件** `resize(4 MiB)` 环形缓冲，而预取相邻曲目
创建的 source 往往从不播放，等于每首白扣 4 MiB。

同时 `MusicPlayer::Track` 的成员顺序（`source`、`duration`、`file`）与它自己的注释相反：
析构顺序是 `file` 先亡、`source` 后亡，而流式解码器正是从该 `file` 读取（WavProf 按需读 64 KiB 窗口），
属于 ARCH-117 同族的悬垂风险。

- [x] 新增 `MusicPlayer::Unload(id)`：停止并销毁单条轨道，立即释放 decoder、流式缓冲与文件，
      同步清理 `current_playing_id_`；返回是否确有该 id。这是 `Load()` 的释放半边。
- [x] `Track` 把 `file` 声明在 `source` 之前，使 `source` 先析构、`file` 后析构，与注释一致；
      `Load` / `LoadFromMemory` 的构造点同步改写，顺序在代码里可见。
- [x] `SDL3StreamingMusicSource` 的环形缓冲改为首次播放时惰性分配（`EnsureRingBuffer()`，
      失败时 `Play()`/`SetPlayingOffset()` 报告失败而不是启动一个没有缓冲的解码线程）；
      `ReleaseBackendHandles()` 释放缓冲，不再把 4 MiB 钉到 source 析构。
- [x] 回归：`Atom_Media.MusicPlayer` 断言 Load/Unload 往返（源计数回到 0、重复 Unload 为
      no-op、卸载正在播放的轨道会清空 now-playing、`Reset()` 释放全部）。
- [ ] 仍待做：`MusicPlayer` 的 `mutex_` 覆盖了整个解码器打开过程（含磁盘 IO），渲染线程调用
      `Play`/`Stop`/`Unload` 时会被一次慢打开阻塞；后续应把慢操作移出锁外，或引入异步加载。
- 约束：轨道级释放必须由调用方（播放器策略）决定驻留窗口，引擎不擅自丢弃 `Load` 过的轨道。

---

## 4. P2：子系统与能力

### 4.1 调度与输入

### CORE-001：固定时间步与调度

> 状态说明：Time 模块已接入主循环，本节前两项已完成；剩余为音频/渲染对齐等后续调度。

- [x] 将逻辑更新与渲染帧率解耦，支持 accumulator、最大追帧次数和插值。（`Time/` 分域时钟 + 固定步 accumulator + max catch-up + alpha；见 `Docs/Plans/Time-System-Plan-CN.md`）
- [x] 明确 Event → Fixed Update → Variable Update → Render → Present 顺序。（`RenderWindow::Run` 已落地）
- [ ] 接入细节：physics domain（120Hz）已创建但未被 `Run()` 消费；alpha 插值系数尚无消费方（依赖精灵/变换状态，归 ECS/渲染范畴）。
- [ ] P3 音频设备钟 `SyncTo`、卡点与 A/V 对齐（设计见 `Docs/Plans/Time-System-Plan-CN.md`）。
- [ ] P4 渲染插值与统一 shader time（同上）。

### CORE-002：InputSystem

- [ ] 建立按帧维护的按下、持续、释放状态。
- [ ] 支持动作映射，避免业务代码直接依赖 SDL keycode。
- 现状：`Event/Input.hpp`（Key/MouseButton/KeyModifiers）与 `Event/ActionMap.hpp`（键位到 Action 的映射）已有雏形，但缺按帧维护的输入状态与多设备合并。

### CORE-004：统一错误模型

- [ ] 为可失败初始化、加载和后端创建统一 Result/Error。
- [ ] 避免混用静默返回、bool、异常和仅日志记录。
- [ ] 错误信息包含模块、操作、资源路径和底层错误。

### 4.1b 并发（Job System，新增 2026-10-11）

> 背景：`Concurrency/` 模块（`Job.hpp` / `WorkStealingDeque.hpp` / `JobSystem.hpp/.cpp`）已落地为引擎
> 通用底层任务调度设施（固定 worker 池 + Chase-Lev work-stealing + 依赖 counter + 三级优先级 + 主线程协助）。
> 首版已通过单测（单 job / 依赖链 / 并发累加 / 主线程协助 / 优先级 / 多线程并发提交 / 嵌套提交 / shutdown）
> 与 200 次压力测试。以下为工业化阶段（step 较小）的后续优化点，按优先级排序。
>
> **2026-10-15 审查修复（保留实施历史）**：
> - [x] `Schedule()` 双重提交竞态（P0）：末尾曾用 `unfinished == 1` 做竞态判断，依赖恰在「注册
>       continuation」与「最终判断」之间完成时会被双重提交、body 执行两次（压力测试 600001/600000 复现）。
>       已改为本地 `already_done_count == valid_deps.size()` 判断，修复后 8/8 次压力测试通过。
> - [x] `Shutdown()` 丢弃 worker 本地队列任务（P1）：原只标记共享池任务 complete，本地队列任务被
>       `queues_.clear()` 直接丢弃、句柄永不 complete。已补齐清空前逐个 `Steal()` 标记 complete。

### CONCURRENCY-001：任务优先级细分与抢占语义（P2）

- [ ] 当前仅 Low/Normal/High 三级，且优先级只影响「入队顺序」、无抢占（运行中的 Normal 不会为 High 让位）。
      对「高优先级必须即时响应」的场景（如音频 underrun 抢救）仍不够；评估是否引入抢占或至少明确并文档化该语义。
- [ ] `Priority` 目前在 `PushToPool` 里按 High/非 High 二分，`Low` 与 `Normal` 实际等价；补齐 Low 的降级调度（空闲时才跑）。

### CONCURRENCY-002：work-stealing 本地队列的调度策略（P2）

- [ ] 首版已接通本地队列（worker 内提交走 lock-free LIFO，跨线程走共享池，窃取走 FIFO），但 `RunOne` 的窃取
      起点固定为 `(idx+1)%n`，无随机化/负载探测；高竞争下可能重复扫描同一队列。评估随机起点或 steal 计数反馈。
- [ ] 本地队列容量固定 256，满时 fallback 共享池；评估动态扩缩容或分块（chunked queue）降低 fallback 频率。

### CONCURRENCY-003：等待语义与忙等消除（P2）

- [ ] `HelpWhileWaiting` 在无可运行 job 时 `std::this_thread::yield()` 忙等；若目标 job 已被 shutdown 丢弃，
      依赖 `running_` 早退防护已避免死锁，但仍有空转。评估引入条件变量/信号量统一等待，减少主线程空转与功耗。
- [ ] `WaitAll` 当前逐次 `WaitFor`（串行 wait），语义正确但非批量；评估一次等待多个 handle 的批量原语。

### CONCURRENCY-004：可观测性与诊断（P2）

- [ ] 暴露 per-worker 队列深度、窃取次数、任务执行时长等统计，供 Debugger overlay 显示（对齐 Log 的 dropped 计数风格）。
- [ ] `PendingJobCount()` 目前是全局原子计数，无法区分「排队中」与「执行中」；补充细分计数。

### CONCURRENCY-005：异常与取消语义（P2/P3）

- [ ] 当前 job 抛异常被 `catch (...)` 静默吞掉，调用方无感知。评估提供 `on_error` 回调或把异常记录到 handle，供诊断。
- [ ] 依赖链上若某一依赖被丢弃（如 shutdown），下游 dependent 的 complete 语义需明确（当前会标 complete，但未执行 body）。

### CONCURRENCY-006：fiber / 同步原语（P3，暂缓）

- [ ] 当前为普通线程 work-stealing，无用户态 fiber。若未来需要「同步等待不阻塞 OS 线程」的协程式调度，
      再评估自研 fiber 或引入成熟库；短期不做。

### CONCURRENCY-007：Shutdown 不解析完整依赖图（P1，新增 2026-10-15）

**背景**：`Shutdown()` 只把「已在队列中的任务」（共享池 + worker 本地队列）标记 complete，但**不会
drain continuation**。一个等待依赖（`unfinished > 1`）的任务从未被提交、不在任何队列里；若其依赖在
shutdown 时被标记 complete 而未通知下游，该任务自身 `complete` 永远为 false，之后 `WaitFor` 永久自旋。

- [ ] shutdown 时沿 continuation 递归标记（或显式清空依赖图），保证「每个已提交句柄最终可观察为完成」。
- [ ] 明确语义：shutdown 丢弃的依赖链上，下游任务应视为「完成（未执行 body）」还是「失败」，并配单测
      覆盖依赖链 + 提前 shutdown 的组合。

### CONCURRENCY-008：Schedule 在 Shutdown 后仍接受提交（P1，新增 2026-10-15）

**背景**：`Schedule()` 不检查 `running_`，文档虽声明 Shutdown 后「停止接受工作」但未强制。此类任务进
共享池后永无 worker 消费，`WaitFor` 命中「未运行」分支的 busy-yield 并永久自旋。

- [ ] 明确并强制语义：Shutdown 后 `Schedule` 返回空句柄（或断言/报错），并在 `WaitFor` 的未运行分支提供
      可终止的等待（如返回 bool 或超时），避免无限自旋。
- [ ] 覆盖「Schedule before Start / between Shutdown and Start」的生命周期边界测试。

### CONCURRENCY-009：诊断计数器下溢与死代码（P3，新增 2026-10-15）

- [ ] `pending_count_` 为 `atomic<std::size_t>`，任何一次重复 `fetch_sub` 会下溢为 SIZE_MAX，污染
      `PendingJobCount()`；改有符号类型或在 `fetch_sub` 前钳制。（重复执行的根因已在 2026-10-15 修复，
      计数器本身的回绕防护仍未做。）
- [ ] `CurrentWorkerIndex()` 是死代码（`SubmitRunnable` 直接读 `thread_local g_worker_index`），删除或改用它。
- [ ] `WorkerCount()` 无同步读 `workers_.size()`，Start/Shutdown 并发调用时是数据竞争；返回稳定快照或加锁。

### 4.2 音频

### AUDIO-001：Mixer / Bus 增益传播

- [!] Beta 阶段暂缓完整 Bus Graph 和 DSP 图。
- [ ] Atom 定义后端无关的 Master、Music、SFX、Voice、UI 逻辑总线。
- [ ] 后端只接收并应用最终增益，不向 `Media/Audio` 暴露 SDL/SFML 类型。
- [ ] 使用脏标记刷新受影响的活动 Source。
- 有效增益：`source gain × category bus gain × master gain`。
- 验收：修改分类音量后，已经播放的 Source 能确定性更新，同时仍允许模块自由组合。

### AUDIO-002：MusicCrossfade 自动化测试

- [ ] 为 Start、Reject、Replace、Cancel、零时长、大 dt、目标缺失和回调重入建立状态机测试。
- [ ] 验证相同 dt 序列产生相同状态和音量结果。

### AUDIO-006：Backend 热切换场景策略与测试

- [x] 切换协议落地为"先建后换 + 通知 + Quiesce + 原子替换 + generation"：创建失败是真回滚，旧后端在替换后才释放，source 由 `IAudioBackend::Quiesce()` / `IAudioSource::Detach()` 统一失效。
- [x] `Audio()` 不再抛异常（无活动后端时返回 `NullAudioBackend`），并提供 `TryAudio()` / `AcquireAudioBackend()`；后台加载线程不再因切换窗口 `std::terminate`。
- [x] 实测通过：播放中切换不再崩溃（原先 0xC0000005 / 堆破坏）、caller-owned source 安全析构、并发 `Audio()` 调用无异常、切换后 ID 由 `GetAudioBackendGeneration()` 判定作废。
- [ ] 在游戏处于正式 Gameplay 场景时禁止切换播放后端。
- [ ] 增加切换成功、未知后端、初始化失败和多个 Player 同时注销的**自动化**测试（当前为临时 harness 手工验证，已删除）。
- [ ] 后续为 Lua 提供受控的 Backend 设置接口；脚本不得直接访问具体 SDL3/SFML 类型。

### AUDIO-007：Music Seek 能力链

- [x] 分层落地：`MusicPlayer::Seek/GetPlayingOffset/GetDuration/IsSeekable` → `IAudioSource::SetPlayingOffset()->bool` + `IsSeekable()` → `IAudioDecoder::SeekToFrame()/IsSeekable()`。
- [x] 内置实现：WavProf（字节偏移）、SDL3Wav（内存游标）、Minimp3（`mp3dec_ex_seek`，采样级）；流式音源按"停线程 → 清缓冲 → 定位 → 重启"实现，buffered 音源直接移动播放游标。
- [x] 实测通过：解码器级 seek 与"从头解码并丢弃"逐字节一致；两个后端上播放中 seek、停止后 seek 再播、越界钳制均正确。
- [ ] 前向-only 解码器的回退策略：当前非 0 目标返回 `false`（只保留回起点），尚未提供"解码并丢弃"的近似实现。
- [ ] 如需 24-bit 内存优化，评估 `AudioSampleFormat::Signed24` 作为解码侧载体 + 推送路径转换（原因与代价见 `Backend/Runtime/README-CN.md`）。（目前不需要）

### AUDIO-008：解码器注册链与职责边界

- [x] `AudioDecoderRegistry` 支持具名候选链：`Register` 首选、`RegisterFallback` 兜底、`Replace` 替换整条链、`CandidatesForFile` 按优先级返回。
- [x] 解码器用 `DecoderOpenStatus`（`Opened`/`UnsupportedFormat`/`InvalidData`/`IoError`）声明失败原因；`AudioClipLoader` 负责尝试循环、引擎格式校验与汇总诊断。
- [x] `.wav` 接入 SDL3 自带解码器（`SDL_LoadWAV`）作为兜底，覆盖 MS ADPCM / IMA ADPCM / A-Law / µ-Law；纯 PCM 仍走流式 WavProf。
- [ ] 补齐自研 WavProf 的压缩 WAV 支持后，移除或对调兜底顺序。
- [ ] 评估为 `.mp3` / 未来格式增加候选（当前 `SDLMIXER_*` 格式解码器全部为 OFF，`MIX_LoadAudio` 在本构建中无法解码任何格式）。

### AUDIO-003：设备 Stream 数量与复用

- [!] 当前并发规模下暂不处理。
- [ ] 对高并发 SFX 做 benchmark，再决定维持独立 stream、建立 stream pool 或使用共享软件混音。

### AUDIO-004：音频格式覆盖

- [!] 后续版本实现 OGG、FLAC 等格式（MP3 已通过 minimp3 支持）。
- [x] 每种格式实现独立 `IAudioDecoder` 并显式注册到 `AudioDecoderRegistry`（`.wav` 双实现 + 候选链，见 AUDIO-008）。
- [x] 队列/依赖说明：SDL3Wav 复用引擎已有的 SDL3 依赖，无新增第三方库与许可证。
- [ ] 选择新的解码器依赖时记录许可证、错误模型和流式解码能力。

### AUDIO-005：Effects 与 Plugins

- [!] Audio Bus、实时线程通信和性能基线稳定前不实施。
- [ ] Effect 与 Transition 分离：Transition 在逻辑帧更新，Effect 在音频 block 上处理 PCM。
- [ ] `IAudioEffect::Process()` 不分配内存、不做文件 I/O、不写同步日志、不持有阻塞 mutex。
- [ ] 先以内置 Gain/Filter 验证接口，再决定是否需要动态库 Plugin。
- [ ] 如引入 Plugin，必须定义 C ABI、版本、所有权、卸载和异常边界。

### 4.3 渲染

### RENDER-001：渲染后端与窗口进一步拆分

- [x] `IWindow`、`IRenderDevice`、`IRenderBackend` 已拆分；`RenderWindow` 只持有组合后的抽象后端。
- [x] 上层不再拥有或包含具体 `SDL3RenderWindow`；默认后端由 `RenderBackendRuntime` 注册为 `sdl_gpu`。
- [ ] 正式 GPU 资源只能由所属 RenderDevice 创建和消费；当前验证场景位于 SDL_GPU 内部 smoke target，不属于公共 RHI。
- [x] Native handle 仅存在于 SDL3 与 SDL_GPU 后端实现内部，不进入公共窗口/RHI 契约。

### RENDER-002：统一 RHI 与 2D/基础 3D Renderer

- [x] 已用 SDL_GPU 自定义 Shader 三角形、纹理 2D quad 与带 depth test 的旋转 3D mesh 验证底层能力。
- [ ] 将验证代码收敛为正式的 Buffer、Texture、Sampler、Shader、Pipeline、CommandBuffer/Pass 资源接口。
- [-] 已支持 source rect、atlas、scale、blend、camera、UTF-8/CJK 基础 text、layer、嵌套裁剪与 batching；rotation/origin、render texture 和复杂 shaping 待实现。
- [ ] 基础 3D 支持 vertex/index mesh、透视相机、depth buffer、纹理和最小 opaque pass；PBR、阴影与 Render Graph 延后。
- [ ] 不继续无限扩张 `IRenderTarget` 虚函数集合。

### RENDER-003：SDL_GPU 首个正式渲染后端

- [-] 作为新渲染架构的首个实现，与 RENDER-001/002 同步推进。
- [x] SDL 根据平台和构建时可用 Shader 格式自动选择 D3D12、Vulkan 或 Metal；Atom 不维护原生 API 选择逻辑。
- [x] GLSL 是唯一人工维护源，CMake 总是生成并校验 SPIR-V，Windows 默认附加 DXIL，macOS 默认附加 MSL。
- [x] 已完成 device/window claim、command buffer、swapchain、render pass、pipeline、buffer、texture、sampler 与 depth 的最小闭环。
- [ ] 将最小闭环重构为可供 Renderer2D/Renderer3D 使用的正式 RHI 资源与命令接口。
- [x] 接入 `imgui_impl_sdlgpu3`，完成 resize、VSync、错误诊断和资源安全销毁的首轮闭环。
- [x] SDL_Renderer 后端实现已从 Atom 构建和源码删除，不提供运行时 fallback。
- 非目标：PBR、阴影、光线追踪、多队列异步计算和首期源码 Shader 热重载。

### RENDER-006：原生 Vulkan 与多后端

- [!] SDL_GPU 完成 2D textured quad、基础 3D mesh，并稳定 RHI 后实施。
- [ ] 复用 SDL 窗口，建立 Vulkan Instance、Device、Surface、Swapchain、descriptor 和同步对象。
- [ ] 复用同一 Shader manifest、Renderer2D 和 Renderer3D，上层仅通过后端 ID `sdl_gpu` / `vulkan` 选择。
- [ ] GPU 资源严格归属创建它的 device；不支持进程运行期间热切换后端。
- [ ] 增加两后端截图一致性、resize、设备错误和资源生命周期测试。

### RENDER-004：跨平台图片解码与纹理上传

- [x] TagLib 元数据字节、`Atom_Image` RGBA 解码和 Renderer2D GPU 上传已拆成独立阶段。
- [x] 使用固定 commit 的 stb 子模块；实现宏所有权、格式覆盖和失败日志记录在 `ImageDecoder` API 中，锁定提交记录在 `ThirdParty/README.md`。
- [ ] Windows、Linux 使用同一份 JPEG/PNG 输入做一致性测试，并覆盖损坏数据、超大尺寸、空封面和不支持格式；WebP 不在 stb_image 支持范围，需要单独 codec adapter。
- [ ] `DecodeImageFile(std::string)` 在 Windows 的非 ASCII 路径仍依赖 C runtime 窄路径行为；资产/VFS 应优先读入字节并调用 `DecodeImageMemory`，后续补 filesystem/IO adapter。

### RENDER-005：Atom 文本与字体系统

- [-] Renderer2D 已增加独立于 ImGui 的内存 Font、UTF-8 解码、stb_truetype 栅格与多页 GlyphAtlas；CJK fallback、文件/VFS/DPI 策略和 HarfBuzz shaping 待实现。
- [x] 字形栅格结果保持后端无关，由 RenderDevice 上传 atlas；公共 API 不暴露 ImGui、SDL 或平台字体类型。
- [x] 图集页首次清零后，新增字形只上传合并后的脏矩形；不再为每个新增字形重传整张页。
- [x] 英文 ASCII 单词按整体换行、tab 对齐至列停靠点、CRLF 的 `\r` 被忽略；缺字尝试显示当前字体的 `.notdef` glyph。
- [ ] 完整 Unicode 换行（UAX #14）、词典断词、双向文本和 locale 规则不能继续堆在 `Renderer2D::ExpandTextOp`；应由 `TextLayout` 负责。
- [ ] `.notdef` 不是 fallback：需要 `FontProvider` 管理字体族、覆盖范围与回退链，并保证度量、baseline 和缓存键与实际选中的字体一致。
- [ ] atlas 尺号已量化到 0.5px，但图集/页面尚无内存预算与 LRU；淘汰必须与 GPU 延迟释放和正在录制的 draw packet 生命周期协同。
- [ ] stb_truetype 仅提供简单轮廓栅格和 kern pair；连字、组合音标、阿拉伯文/Indic 重排、彩色 emoji、OpenType feature/variation/DPI 实例应由 FreeType + HarfBuzz 路径提供。
- 现状：`atom::debugger::FontLoader` 只修复 Debugger 的 ImGui font atlas，不是 Atom Renderer 的字体实现，不应被正式游戏 UI 依赖。

### RENDER-007：SDLGPU 内部组件化

- [x] 完成 `Device`、`Encoding`、`Pipelines`、`Debug` 目录边界；共享 shader loader 与内置 2D pipeline factory 已落地。
- [ ] 将 `SDLGPUDevice` 内部的 ResourceStore 拆出，集中管理 texture、buffer、sampler、upload 和延迟释放。
- [ ] 将 CommandEncoder/RenderPass 编码从设备生命周期中拆出，设备只负责 device、window、swapchain 和 frame 状态。
- [ ] 保持所有 SDL_GPU native 类型留在 `Backend/SDLGPU`，不得泄漏到 `Render/` 或公共 RHI。

### RENDER-008：通用可附加 Shader Effect

- [ ] 用资源驱动的 `RenderEffect`/fullscreen pass 替代 `PostProcess2DEffect` 固定枚举和后端 `switch`。
- [ ] 支持 effect attach 到任意 RenderTarget/RenderPass，背景、卡片、文字和调试器互不误渲染。
- [ ] 增加 shader binding/uniform reflection 与布局 hash 校验；缺失变体必须返回明确错误。
- [ ] Gaussian、Glitch、Chromatic Aberration 迁移为普通内置 shader 包，保持旧 API 兼容包装直到迁移完成。

### RENDER-009：正式通用 RHI 与 3D

- [ ] 将当前 `IRender2DContext` 降级为兼容适配器，新增正式的 Buffer、Texture、Sampler、Shader、Pipeline、RenderTarget 和 CommandEncoder 契约。
- [ ] 让 Renderer2D/Renderer3D 只生成 DrawPacket、Material 和 RenderPass，不直接选择 SDL_GPU pipeline。
- [ ] 完成 mesh/index、透视相机、depth、材质和最小 opaque pass 的跨后端闭环；不得把 3D 特例加入 `SDLGPU2D.cpp`。

### RENDER-010：生产级文字后端

- [ ] 将现有 stb 字体路径继续封装为 `FontProvider`、`TextLayout`、`GlyphAtlas`、`TextRenderer`，Renderer2D 不再直接解析 UTF-8 或管理字体文件。
- [ ] 评估并接入 FreeType + HarfBuzz，用于 CJK fallback、kerning、组合字符、阿拉伯文/Indic shaping 和 DPI 实例缓存。
- [ ] 保留 stb_truetype 作为轻量构建或无 shaping fallback，统一错误和资源生命周期。

### RENDER-011：渲染回归护栏

- [ ] 增加 Null/Recording Render Backend，验证 frame/pass/资源顺序和释放协议。
- [ ] 增加 shader reflection/layout 校验、资源生命周期测试和截图 golden test，减少对人工验收的依赖。
- [ ] 覆盖 SDL_GPU 的 D3D12、Vulkan、Metal 运行时 shader variant 选择和区域后处理路径。

### 4.4 模块边界与构建

### CORE-003：目录与命名收敛

- [ ] 继续清理空目录和错误命名；`Render/Shader/TODO` 空占位文件已删除。
- [x] 已删除早期 `Config` 目录；引擎配置应归属于各自的模块，而非新的通用目录。
- [ ] 明确 Public/Internal/Backend 的头文件边界。
- [ ] **空壳模块清理（新增，2026-09-29）**：`Media/Video/Video.hpp` 是空壳且命名空间错误（`engine::video`，应为 `atom::video`），`Physics/` 只有 README。空壳模块应标为 Experimental 或移除，见 CORE-006。

### CORE-005：CMake target 与可移植性

- [-] 清理全局 include/link directories，改为 target 级依赖。（渲染链已拆为 `Atom_Backend_SDL3_Window`、`Atom_Backend_SDL_GPU` 与 `Atom_Backend_Render_Runtime`。）
- [ ] 明确 PUBLIC/PRIVATE/INTERFACE 传播边界。
- [x] Shader 工具优先由 `VULKAN_SDK`/`PATH` 发现；SDK 根目录只允许作为本机 CMake cache 提示，仓库不保存绝对路径。
- [ ] 增加 install/export/package config 和 `Atom::*` 导出目标。

### CORE-006：未实现模块的正式 API 管理

- [ ] Video 等空壳模块在实现前标为 Experimental，或不进入稳定公共 API。
- [ ] 每个公开模块至少具备最小能力、错误返回和示例。
- 现状：`Media/Video/Video.hpp` 为空壳类且命名空间错误，需一并处理。

### CORE-008：外部 Shader 工具链

- [ ] 提供独立 `atom-shaderc`，读取用户自己的 `.atomshader` manifest、GLSL 源码和输出目录。
- [ ] 用户新增 shader 不得修改 Atom 内部 `Render/Shader/AtomShaders.cmake`；引擎只维护内置 shader 默认包。
- [ ] 工具负责 GLSL → SPIR-V，并按目标平台生成可选 DXIL/MSL 变体及 reflection metadata。
- [!] 本轮暂不实现编译器和外部项目 CMake 集成，先冻结资源包格式与公共 pipeline 描述。
- 关联：圆角 SDF 蒙版「可插拔」拆分与 shader 通用/专属分层的完整设计见 `Docs/Plans/Shader-Extension-Plan-CN.md`（职责边界：游戏侧不得反向修改引擎侧 `AtomShaders.cmake`）。

### CORE-009：Utilities 目录职责与命名

- `Utilities/Utf8` 当前只负责 UTF-8 ↔ 宽字符转换，名称准确，暂不为了「看起来统一」改名。
- 如果后续扩展为 UTF-16、locale、路径规范化等完整编码服务，再整体迁移为 `Utilities/Encoding`，同时统一 target/API 和所有 include；不要只改目录名。
- `Utilities/Packager` 继续作为运行时/工具共用的资源打包模块；Utilities 按职责拆分，不建立一个集中式杂物目录。
- `atom-shaderc` 属于开发工具链，未来应放在 `Tools/Shaderc`（或独立工具仓库），不放进运行时 `Utilities`；它的输出是资源包，不是 Atom 核心库依赖。

---

## 5. P3：工程化与产品化

### QUALITY-001：自动化测试与性能基线

- [ ] 建立最小测试 target 与 CTest。
- [ ] 覆盖 Screen 生命周期、事件单次分发、音频 Registry、VoicePool、MusicCrossfade、Lua 错误路径和 Packager 边界。
- [ ] 建立音频并发、渲染缓存和主循环帧时间 benchmark。

### QUALITY-002：多平台 CI 与质量门禁

- 已有基础门禁：目标为 `master` 的 PR 会执行 Windows MSVC 与 Linux GCC 构建。
- [ ] 增加 Windows MinGW 与 Linux Clang 构建。
- [ ] clang-format、clang-tidy 或等效静态分析。
- [ ] AddressSanitizer、UndefinedBehaviorSanitizer；可用平台增加 ThreadSanitizer。
- [ ] 输出测试覆盖率报告。

### PRODUCT-001：SDK 与版本策略

- [ ] 公共 API/ABI 兼容策略。
- [ ] APKG 格式版本与兼容策略。
- [ ] CMake 安装、导出和消费者示例。

### PRODUCT-002：资产与开发工具

- [ ] 资源 import/cook、异步构建、依赖图和增量构建。
- [ ] Profiler、帧统计、Editor/Inspector。
- [ ] 安全的脚本与资源热重载。

### PRODUCT-003：动态链接发布配置

- [ ] 在公共 ABI、句柄所有权、异常/运行库策略稳定后，增加 `ATOM_BUILD_SHARED` 与 install/export 配置。
- [ ] 明确 Windows DLL、Linux SO、macOS dylib 的依赖部署、符号导出和版本策略。
- [ ] shader 资源保持独立包格式；C/C++ 动态链接不会改变 shader 的运行时加载方式。
- [!] 不把动态链接作为当前 SDL_GPU/RHI 重构前置条件，避免在 ABI 未冻结时引入部署和跨模块内存问题。

### CORE-007：源码许可证标头统一

- [ ] 按照统一的 MIT SPDX 规则为 Atom 自有源码统一标识。
- [ ] 删除旧的 `All rights reserved`，不修改 `ThirdParty/` 中的上游标头。
- [ ] 将标头迁移作为独立机械提交，避免与功能改动混合。
- [ ] 未来处理时使用真实作者和可确认的首次年份；无法确认时不凭空推测。
- [ ] 未来处理时保留 `@file`/`@brief` 等有价值说明，不强制 `@author`、`@date` 和空的 `@attention`。
- 当前审计结果：Atom 自有 C/C++ 文件 161 个，其中 45 个仍含旧标头，9 个已有 SPDX。

```powershell
rg -l "All rights reserved" -g "*.h" -g "*.hpp" -g "*.c" -g "*.cpp" -g "!ThirdParty/**"
rg -L "SPDX-License-Identifier: MIT" -g "*.h" -g "*.hpp" -g "*.c" -g "*.cpp" -g "!ThirdParty/**"
```

---

## 6. 已接受的架构基线

以下决策原先分散在各路线文档中，现集中维护于本清单：

1. 首个正式渲染后端是 SDL_GPU；由 SDL 根据平台和设备选择 D3D12、Vulkan 或 Metal，Atom 不复制一套底层 API 选择逻辑。
2. SDL_Renderer、SDL_Texture 和 `imgui_impl_sdlrenderer3` 不再作为 Atom 运行时后端；无 GPU 测试应使用未来的 Null/Recording Backend。
3. RHI 同时面向 2D 和基础 3D，公共头文件不得暴露 SDL_GPU、Vk、D3D 或 Metal 类型。Renderer2D/Renderer3D 只产生渲染意图，后端负责执行。
4. GLSL 是唯一人工维护的 shader 源语言，优先生成 SPIR-V；DXIL/MSL 是可选离线变体。shader 资源与 Atom C/C++ 链接方式无关。
5. 原生 Vulkan 是后续并列后端，必须复用同一套资源描述、shader 包和上层 Renderer；进程运行期间不支持后端热切换，GPU 资源不得跨 device 共享。
6. SDL_GPU 每帧顺序固定为：获取 command buffer → 获取 swapchain → 上传动态数据 → 3D → 2D/UI → Debugger → 提交。最小化或暂时没有 swapchain texture 时跳过该帧，不忙等。
7. **后端分层规则**：上层只消费 Contract + Registry；Runtime 是唯一认识具体后端的层；Contract 头文件不得暴露任何后端 native 类型。

---

## 7. 文档归档规则

- 本文件是 Atom 架构整改、渲染路线、许可证迁移和后续任务的唯一汇总入口。
- 具体模块 API 和使用方式仍放在模块 README（例如 `Render/Text/README-CN.md`、`Layout/README.md`）。
- 已完成任务的详细排查过程不再单独维护历史报告；如其中存在可复用的根因或验收命令，应提炼后写入本文件。
- **Example 边界**：`Example/` 下的应用（如 MusicCard）问题不进入本文件；引擎侧被示例暴露出的通用缺陷才在此记录。

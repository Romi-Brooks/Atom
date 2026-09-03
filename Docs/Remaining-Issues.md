# Atom 未完成工作统一清单

> 状态：唯一有效的架构整改与后续规划文档
> 更新日期：2026-09-01

## 1. 使用规则

- `[ ]` 未开始
- `[-]` 处理中
- `[x]` 已完成（保留实施历史，不删除）
- `[!]` Beta 阶段暂缓
- API、编码规范和使用说明继续保留在各自稳定文档中。

## 2. P0：稳定性与确定性问题

### ARCH-004：引擎事件可能被扩展层重复处理

- [x] 确认 `SDL3RenderWindow::PollEvent()` 与 `RenderWindow::ProcessEvents()` 的回调链，确保一个 SDL 事件只转换和分发一次。（已修复：`PollEvent` 回归纯取事件原语，`RenderWindow::ProcessEvents` 唯一分发并带 None 守卫，见 2026-08-16 提交。）
- [x] 原始 SDL 事件仅提供给平台适配/ImGui，引擎事件只在上层分发。（已落地：`IRenderWindow::SetRawEventHook` 只服务平台适配器；引擎事件经 `IEvent` 在门面层分发。）
- 验收：单次键盘、鼠标和窗口事件不会触发两次业务回调。

### ARCH-005：Lua 错误处理与空指针保护

- [ ] 检查 `lua_tostring()` 返回空指针的情况，完整记录错误字符串。
- [ ] 为 Lua 调用增加 stack guard，保证失败后栈平衡。
- [ ] Music/SFX/Mixer/Transition 注入对象为空或失效时返回受控 Lua 错误。
- 验收：语法错误、运行时错误、非字符串错误对象和空注入均不崩溃。

### ARCH-006：公共数值参数缺少统一边界契约

- [ ] 明确 `SetFPS(0)` 的含义并避免除零。
- [ ] 校验圆形半径、音频 offset、voice 上限和潜在大内存参数。
- [ ] 统一音量范围和非法参数处理方式。
- 验收：非法输入不会造成崩溃、除零、溢出或异常大分配。

## 3. P1：核心架构与生命周期

### ARCH-101：可选的运行时组合层

- [!] Beta 阶段不引入强制 `AudioSystem` 或完整 `EngineRuntime`。
- [ ] 后续根据统一初始化、失败回滚、停止请求、headless 测试和服务生命周期的实际需求，评估可选 `ApplicationRunner/EngineRuntime`。
- 约束：不得破坏用户独立组合 Music、SFX、Mixer 和 Transition 的能力。

### ARCH-105：ScreenManager 生命周期安全

- [ ] 将 current/stack 的裸指针改为 ScreenId、generation handle 或其他可验证句柄。
- [ ] 禁止静默覆盖 active/stack 中的同名 Screen。
- [ ] 为重复注册、卸载和切换返回明确结果。
- 验收：替换或卸载 Screen 后不存在悬空引用。

### ARCH-106：Circle Texture Cache 无界增长

- [ ] 对半径进行量化，设置容量上限或 LRU。
- [ ] 评估单位圆纹理、几何渲染或 shader，避免动态半径持续创建纹理。
- [ ] 不永久缓存失败项。
- 验收：动态圆形场景中缓存和 GPU 内存保持有界。

### ARCH-107：统一资源系统与 VFS

- [ ] 设计 Resource ID、`ResourceHandle<T>`、Loader Registry 和统一缓存。
- [ ] 支持目录与 APKG 的透明挂载。
- [ ] 后续增加异步加载、热重载、依赖图和内存预算。
- 验收：Texture、AudioClip、Script 可通过统一 URI 加载并共享资源。

### ARCH-108：Entity 职责拆分

- [ ] 分离 Transform、Health、Renderable、Movement/Combat 等数据与系统。
- [ ] 移除 `const Move()` 通过 mutable 修改状态的设计。
- [ ] Entity 数据不直接持有或加载具体渲染后端资源。
- 方向：从简单 component pool 开始，不直接引入复杂 archetype ECS。

### ARCH-109：Lua 对象生命周期与隔离

- [ ] 用 `LuaContext`、userdata/upvalue 或可验证 handle 替代命名空间全局裸指针。
- [ ] Entity 绑定使用 ID + generation，避免暴露裸指针。
- [ ] 保证重复 Initialize、Shutdown 和热重载安全。
- [ ] 后续增加内存限制和可选标准库沙箱。

### ARCH-110：日志并发与实时路径

- [ ] 日志级别使用原子或同步访问。
- [ ] 使用线程安全时间格式化，Channel 避免无意义复制。
- [ ] 音频实时路径不得同步写控制台或持有阻塞锁。
- [ ] 后续提供 sink、异步队列和 Release 日志裁剪。

### ARCH-112：扩展回调由单槽改为 Listener Registry

- [x] Event、Update、Overlay、Shutdown 使用 token/RAII Connection 注册。（已落地：`RenderWindow` 提供 `Add*Listener` + `ListenerConnection`，见 2026-08-16。）
- [x] Debugger、Profiler、Console 和用户 Overlay 可以并存。（监听器层已可并存；ImGui 层限制见 ARCH-113。）
- [x] 注销一个监听器不得清空其他监听器。（已落地：按 id 独立注销。）
- 约束：监听器不得在自身被分发期间注销。

### ARCH-113：多个 ImGui Overlay 需要共享上下文

- [ ] ImGui 全局上下文按窗口共享一份（OverlayManager），Debugger/Profiler/Console 只贡献 `OnDrawOverlay` 内容。
- [ ] 现状：每个 `Debugger::Attach` 独立 `ImGui::CreateContext()`，同窗口挂两个 ImGui Debugger 会互相覆盖渲染（监听器层已可并存，ImGui 层尚不能）。
- 验收：同一窗口可挂多个 ImGui Overlay 而不互相覆盖。

### ARCH-114：Packager 路径与编码加固

- [x] 跨盘打包导致包内文件名为空 → 已修复：`fs::relative` 跨盘返回空路径（libstdc++）或抛异常（MSVC），`GenerateInternalFilename` 现在 fallback 到裸文件名。（2026-08-16 实测 + 修复）
- [ ] 路径统一走宽字符转换（复用 `Utf8ToWide`/`Utf8FromWide`）：`std::filesystem` 窄字符串按实现定义的字符集解释（MinGW 按 UTF-8 透传、MSVC 按 ANSI 代码页），保证中文/非 ASCII 文件名跨编译器稳定。当前 MinGW 下 UTF-8 输入全链路正常（实测通过）。
- [ ] `packager_tool` 交互输入：中文 Windows 控制台 stdin 为 GBK 字节，与文件系统的 UTF-8 语义冲突（实测报 `Illegal byte sequence`）；读入后按 UTF-8 期望或显式转码。

## 4. 音频后续事项

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

- [ ] 在游戏处于正式 Gameplay 场景时禁止切换播放后端。
- [ ] 主菜单/设置页面切换后，由页面或下一场景重新注册并播放所需 ID。
- [ ] 增加切换成功、未知后端、初始化失败、旧后端恢复失败和多个 Player 同时注销的测试。
- [ ] 后续为 Lua 提供受控的 Backend 设置接口；脚本不得直接访问具体 SDL3/SFML 类型。

### AUDIO-003：设备 Stream 数量与复用

- [!] 当前并发规模下暂不处理。
- [ ] 对高并发 SFX 做 benchmark，再决定维持独立 stream、建立 stream pool 或使用共享软件混音。

### AUDIO-004：音频格式覆盖

- [!] 后续版本实现 OGG、FLAC 等格式（MP3 已通过 minimp3 支持，见 `Backend/Builtin/Audio/Decoder/minimp3/Minimp3Decoder`）。
- [ ] 每种格式实现独立 `IAudioDecoder` 并显式注册到 `AudioDecoderRegistry`。
- [ ] 选择依赖时记录许可证、错误模型和流式解码能力。

### AUDIO-005：Effects 与 Plugins

- [!] Audio Bus、实时线程通信和性能基线稳定前不实施。
- [ ] Effect 与 Transition 分离：Transition 在逻辑帧更新，Effect 在音频 block 上处理 PCM。
- [ ] `IAudioEffect::Process()` 不分配内存、不做文件 I/O、不写同步日志、不持有阻塞 mutex。
- [ ] 先以内置 Gain/Filter 验证接口，再决定是否需要动态库 Plugin。
- [ ] 如引入 Plugin，必须定义 C ABI、版本、所有权、卸载和异常边界。

## 5. 渲染与窗口后续事项

### RENDER-001：渲染后端与窗口进一步拆分

- [ ] 将窗口管理与 Renderer/RenderDevice 分离。
- [x] 消除上层对具体 `SDL3RenderWindow` 的所有权依赖。（已落地：`RenderWindow` 持有 `IRenderWindow*`，经 `RenderBackendRegistry` + `RenderBackendRuntime` 创建，见 2026-08-16。）
- [ ] 纹理由所属 RenderDevice 创建和消费，禁止依赖跨后端 `dynamic_cast`。（现状记录：`SDL3RenderWindow::DrawTexture` 仍 `dynamic_cast<SDL3Texture&>`；且 Atom 尚未实现纹理/渲染功能主体——`ITexture` 无创建工厂、`Entity::texture_` 无赋值路径。此条随 RENDER-002 与纹理工厂实施时一并解决，Vulkan 进场前必须落地。）
- [x] Native handle 只存在于后端专用扩展接口。（已落地：`ISDL3WindowExtensions`，`IRenderWindow` 不再暴露 `void*`，见 2026-08-16。）

### RENDER-002：完整 2D Renderer

- [ ] 规划 `RenderDevice + Renderer2D + RenderQueue + RenderPass`。
- [ ] 支持 source rect、atlas、scale、rotation、origin、blend、camera、text、render texture、layer 与 batching。
- [ ] 不继续无限扩张 `IRenderTarget` 虚函数集合。

### RENDER-003：Vulkan 2D 后端

- [!] 在 RENDER-001 抽象稳定后实施。
- [ ] CMake 集成 Vulkan SDK，建立 Instance、Device、Surface、Swapchain 和同步对象。
- [ ] 实现 Clear/Present、纹理上传、DrawTexture、DrawRect、DrawCircle。
- [ ] 集成 ImGui Vulkan、Resize/Swapchain 重建和设备丢失恢复。
- [ ] 保留 SDL3 Renderer 作为默认或 fallback，并提供明确后端选择机制。
- 非目标：3D、光线追踪、多队列异步计算、Beta 阶段 shader 热重载。

### RENDER-004：跨平台图片解码与纹理上传

- [ ] 将“媒体元数据提取”“图片解码”“GPU 纹理上传”拆成三个边界清晰的阶段：TagLib/Media 层只返回封面编码字节与 MIME；独立 ImageCodec/Asset 模块输出后端无关的 RGBA 像素缓冲、尺寸及颜色/Alpha 信息；RenderDevice 只负责上传像素并创建所属后端纹理。
- [ ] 评估 SDL_image、stb_image 或自有 codec adapter，记录许可证、内存解码、格式覆盖、错误模型和线程安全；WIC 等平台原生解码器只作为可选适配器，不能成为公共 API 契约。
- [ ] Windows、Linux 使用同一份 JPEG/PNG/WebP 输入做一致性测试，并覆盖损坏数据、超大尺寸、空封面和不支持格式。
- 当前 `Example_Animated_Music_Card` 中的 WIC → SDL_Texture 路径只是 Windows 概念验证；非 Windows 使用生成式 fallback。后续不得把 WIC 或 SDL 纹理职责并入 TagLib 元数据封装。

### RENDER-005：Atom 文本与字体系统

- [ ] 正式 Renderer 增加独立于 ImGui 的 FontAsset/FontFace、GlyphAtlas、TextLayout 与 TextRenderer；支持 UTF-8、CJK fallback、文件/内存/VFS 加载、DPI、字形缓存，并评估 HarfBuzz 等 shaping 方案。
- [ ] 字形栅格结果保持后端无关，由 RenderDevice 上传 atlas；公共 API 不暴露 ImGui、SDL 或平台字体类型。
- 当前 `atom::debugger::ImGuiFontLoader` 只修复 Debugger 的 ImGui font atlas，支持文件与内存字体；它不是 Atom Renderer 的字体实现，也不应被正式游戏 UI 依赖。
- 在正式文本渲染器完成前，Example/Debugger 可继续使用当前桥接方案验证中文显示。

## 6. 调度、输入与模块边界

### CORE-001：固定时间步与调度

- [ ] 将逻辑更新与渲染帧率解耦，支持 accumulator、最大追帧次数和插值。
- [ ] 明确 Event → Fixed Update → Variable Update → Render → Present 顺序。

### CORE-002：InputSystem

- [ ] 建立按帧维护的按下、持续、释放状态。
- [ ] 支持动作映射，避免业务代码直接依赖 SDL keycode。

### CORE-003：目录与命名收敛

- [ ] 继续清理空目录、TODO 占位目录和错误命名。
- [ ] 将 `Config` 中的 C++ 组件类型迁回领域目录；Config 只保留配置数据。
- [ ] 明确 Public/Internal/Backend 的头文件边界。

### CORE-004：统一错误模型

- [ ] 为可失败初始化、加载和后端创建统一 Result/Error。
- [ ] 避免混用静默返回、bool、异常和仅日志记录。
- [ ] 错误信息包含模块、操作、资源路径和底层错误。

### CORE-005：CMake target 与可移植性

- [-] 清理全局 include/link directories，改为 target 级依赖。（部分落地：渲染链已显式化——`Atom_Backend_SDL3_Render` 显式链接 `Atom_Algorithm`，`Atom_Window` 经 `Atom_Backend_Render_Runtime` 与具体后端解耦，见 2026-08-16。）
- [ ] 明确 PUBLIC/PRIVATE/INTERFACE 传播边界。
- [ ] 消除本机绝对路径与平台隐式依赖。
- [ ] 增加 install/export/package config 和 `Atom::*` 导出目标。

### CORE-006：未实现模块的正式 API 管理

- [ ] Video 等空壳模块在实现前标为 Experimental，或不进入稳定公共 API。
- [ ] 每个公开模块至少具备最小能力、错误返回和示例。

### CORE-007：源码许可证标头统一

- [ ] 按照 [`Source-Header-Migration-CN.md`](Source-Header-Migration-CN.md) 为 Atom 自有源码统一 MIT SPDX 标识。
- [ ] 删除旧的 `All rights reserved`，不修改 `ThirdParty/` 中的上游标头。
- [ ] 将标头迁移作为独立机械提交，避免与功能改动混合。

## 7. 测试与产品化

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

## 8. 建议实施顺序

1. P0 事件、Lua 错误和参数校验。
2. 最小测试 target，优先覆盖 MusicCrossfade 与 Screen/Event。
3. Screen 生命周期、回调 Registry、Lua handle 和日志并发。
4. 资源系统、固定时间步和 InputSystem。
5. Renderer2D 抽象稳定后再实现 Vulkan。
6. Mixer/Bus、Effects/Plugins 按实际 Beta 需求启用。
7. 最后推进 CI、SDK、资产管线和编辑器能力。

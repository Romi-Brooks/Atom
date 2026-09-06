D:\Project\Repo\Atom\Docs\Remaining-Issues.md# Atom 未完成工作统一清单

> 状态：唯一有效的架构整改与后续规划文档
> 更新日期：2026-09-04

## 1. 使用规则

- `[ ]` 未开始
- `[-]` 处理中
- `[x]` 已完成（保留实施历史，不删除）
- `[!]` Beta 阶段暂缓

## 2. P0：稳定性与确定性问题

### ARCH-004：引擎事件单次分发

- [x] `SDL3Window::PollEvent()` 只做原始事件 hook 与单次转换，`RenderWindow::ProcessEvents()` 负责唯一一次引擎分发。
- [x] 未映射的 SDL 事件不再以 `EventType::None` 发送给 Screen。
- [x] `SDL_EVENT_QUIT` 与 `SDL_EVENT_WINDOW_CLOSE_REQUESTED` 都转换为关闭事件。
- [x] 平台事件在 SDL3 后端转换为规范化 `IEvent`；普通业务与 ImGui 桥接层均不接收原始 SDL 指针。

### ARCH-005：Lua 错误处理与空指针保护

- [ ] 检查 `lua_tostring()` 返回空指针的情况，完整记录错误字符串。
- [ ] 为 Lua 调用增加 stack guard，保证失败后栈平衡。
- [ ] Music/SFX/Mixer/Transition 注入对象为空或失效时返回受控 Lua 错误。
- 验收：语法错误、运行时错误、非字符串错误对象和空注入均不崩溃。

### ARCH-006：公共数值参数缺少统一边界契约

- [x] `SetFPS(0)` 明确表示不做软件限帧，计时器不会除零；重新初始化窗口会恢复门面层保存的 FPS 设置。
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
- [x] Debugger、Profiler、Console 和用户 Overlay 可以并存。（共享 ImGui context 已落地，见 ARCH-113。）
- [x] 注销一个监听器不得清空其他监听器。（已落地：按 id 独立注销。）
- 约束：监听器不得在自身被分发期间注销。

### ARCH-113：多个 ImGui Overlay 需要共享上下文

- [x] 由窗口级 `OverlayManager` 共享一份 ImGui 上下文；Debugger/LogDebugger/未来的 Profiler/Console 只贡献 panel 内容。
- [x] 旧 SDL_Renderer ImGui 适配器已退出 Atom target，SDL_GPU 的 `imgui_impl_sdlgpu3` 已注册并通过单 Debugger 验收。
- [x] `Log` 提供线程安全的 RAII subscription；`Debugger` 可通过 `SetLoggerEnabled()` 挂载 LogDebugger，使用独立缓冲区和主线程 ImGui 绘制。
- 验收：同一窗口可挂多个 ImGui Overlay 而不互相覆盖。（已通过共享 Debugger + LogDebugger 示例构建验收。）

### ARCH-114：Packager 路径与编码加固

- [x] 跨盘打包导致包内文件名为空 → 已修复：`fs::relative` 跨盘返回空路径（libstdc++）或抛异常（MSVC），`GenerateInternalFilename` 现在 fallback 到裸文件名。（2026-08-16 实测 + 修复）
- [x] 路径统一走宽字符转换（复用 `Utf8ToWide`/`Utf8FromWide`）：`Atom_UTF8` 现提供 `PathFromUtf8`/`PathToUtf8`，MusicCard、Packager、Unpackager 的路径字符串边界统一转换（2026-09-04）。
- [ ] `packager_tool` 交互输入：中文 Windows 控制台 stdin 为 GBK 字节，与文件系统的 UTF-8 语义冲突（实测报 `Illegal byte sequence`）；读入后按 UTF-8 期望或显式转码。

### ARCH-115：ImGui 原生 Multi-Viewport

- [ ] 启用 `ImGuiConfigFlags_ViewportsEnable`，并补齐 SDL3 platform backend 的 viewport 创建、销毁、移动和输入路由。
- [ ] 为 SDL_GPU renderer backend 实现每个 viewport 的 swapchain/render target 和 draw-data 提交。
- [ ] 在主帧之外调用 platform window update/render 生命周期，并验证资源同步、resize、DPI 和 shutdown。
- 这不是 ARCH-113 的“共享一个 ImGui context”本身；ARCH-113 解决同一主窗口内的多个 ImGui window，ARCH-115 才是可拖出为原生 SDL 窗口。

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

- [!] 后续版本实现 OGG、FLAC 等格式（MP3 已通过 minimp3 支持，见 `Backend/Audio/Decoder/minimp3/Minimp3Decoder`）。
- [ ] 每种格式实现独立 `IAudioDecoder` 并显式注册到 `AudioDecoderRegistry`。
- [ ] 选择依赖时记录许可证、错误模型和流式解码能力。

### AUDIO-005：Effects 与 Plugins

- [!] Audio Bus、实时线程通信和性能基线稳定前不实施。
- [ ] Effect 与 Transition 分离：Transition 在逻辑帧更新，Effect 在音频 block 上处理 PCM。
- [ ] `IAudioEffect::Process()` 不分配内存、不做文件 I/O、不写同步日志、不持有阻塞 mutex。
- [ ] 先以内置 Gain/Filter 验证接口，再决定是否需要动态库 Plugin。
- [ ] 如引入 Plugin，必须定义 C ABI、版本、所有权、卸载和异常边界。

## 5. 渲染与窗口后续事项

> 已接受的渲染架构、SDL_GPU 实施路线、基础 3D 边界与原生 Vulkan 多后端计划见本文第 9 节；本文件是唯一有效的实施基线。

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
- [x] MusicCard 与 Debugger 已迁移；Atom 源码和 target 不再使用 `SDL_Renderer`、`SDL_Texture` 或 `imgui_impl_sdlrenderer3`（上游源码树保留未编译 backend 文件）。
- 非目标：PBR、阴影、光线追踪、多队列异步计算和首期源码 Shader 热重载。

### RENDER-006：原生 Vulkan 与多后端

- [!] SDL_GPU 完成 2D textured quad、基础 3D mesh，并稳定 RHI 后实施。
- [ ] 复用 SDL 窗口，建立 Vulkan Instance、Device、Surface、Swapchain、descriptor 和同步对象。
- [ ] 复用同一 Shader manifest、Renderer2D 和 Renderer3D，上层仅通过后端 ID `sdl_gpu` / `vulkan` 选择。
- [ ] GPU 资源严格归属创建它的 device；不支持进程运行期间热切换后端。
- [ ] 增加两后端截图一致性、resize、设备错误和资源生命周期测试。

### RENDER-004：跨平台图片解码与纹理上传

- [x] TagLib 元数据字节、`Atom_Image` RGBA 解码和 Renderer2D GPU 上传已拆成独立阶段；MusicCard 不再使用 WIC/SDL_Texture。
- [x] 使用固定 commit 的 stb 子模块；实现宏所有权、格式覆盖和失败日志记录在 `ImageDecoder` API 中，锁定提交记录在 `ThirdParty/README.md`。
- [ ] Windows、Linux 使用同一份 JPEG/PNG 输入做一致性测试，并覆盖损坏数据、超大尺寸、空封面和不支持格式；WebP 不在 stb_image 支持范围，需要单独 codec adapter。
- [ ] `DecodeImageFile(std::string)` 在 Windows 的非 ASCII 路径仍依赖 C runtime 窄路径行为；资产/VFS 应优先读入字节并调用 `DecodeImageMemory`，后续补 filesystem/IO adapter。

### RENDER-005：Atom 文本与字体系统

- [-] Renderer2D 已增加独立于 ImGui 的内存 Font、UTF-8 解码、stb_truetype 栅格与多页 GlyphAtlas；CJK fallback、文件/VFS/DPI 策略和 HarfBuzz shaping 待实现。
- [x] 字形栅格结果保持后端无关，由 RenderDevice 上传 atlas；公共 API 不暴露 ImGui、SDL 或平台字体类型。
- 当前 `atom::debugger::ImGuiFontLoader` 只修复 Debugger 的 ImGui font atlas，支持文件与内存字体；它不是 Atom Renderer 的字体实现，也不应被正式游戏 UI 依赖。
- MusicCard 业务文字已使用 Renderer2D；ImGuiFontLoader 只给调试窗口提供字体。

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
- [ ] 增加 shader reflection/layout 校验、资源生命周期测试和截图 golden test，减少对 MusicCard 人工验收的依赖。
- [ ] 覆盖 SDL_GPU 的 D3D12、Vulkan、Metal 运行时 shader variant 选择和区域后处理路径。

## 6. 调度、输入与模块边界

### CORE-001：固定时间步与调度

- [ ] 将逻辑更新与渲染帧率解耦，支持 accumulator、最大追帧次数和插值。
- [ ] 明确 Event → Fixed Update → Variable Update → Render → Present 顺序。

### CORE-002：InputSystem

- [ ] 建立按帧维护的按下、持续、释放状态。
- [ ] 支持动作映射，避免业务代码直接依赖 SDL keycode。

### CORE-003：目录与命名收敛

- [ ] 继续清理空目录和错误命名；本轮已删除 `Render/Shader/TODO` 空占位文件。
- [x] 已删除早期 `Config` 目录；引擎配置应归属于各自的模块，而非新的通用目录。
- [ ] 明确 Public/Internal/Backend 的头文件边界。

### CORE-004：统一错误模型

- [ ] 为可失败初始化、加载和后端创建统一 Result/Error。
- [ ] 避免混用静默返回、bool、异常和仅日志记录。
- [ ] 错误信息包含模块、操作、资源路径和底层错误。

### CORE-005：CMake target 与可移植性

- [-] 清理全局 include/link directories，改为 target 级依赖。（渲染链已拆为 `Atom_Backend_SDL3_Window`、`Atom_Backend_SDL_GPU` 与 `Atom_Backend_Render_Runtime`。）
- [ ] 明确 PUBLIC/PRIVATE/INTERFACE 传播边界。
- [x] Shader 工具优先由 `VULKAN_SDK`/`PATH` 发现；SDK 根目录只允许作为本机 CMake cache 提示，仓库不保存绝对路径。
- [ ] 增加 install/export/package config 和 `Atom::*` 导出目标。

### CORE-008：外部 Shader 工具链

- [ ] 提供独立 `atom-shaderc`，读取用户自己的 `.atomshader` manifest、GLSL 源码和输出目录。
- [ ] 用户新增 shader 不得修改 Atom 内部 `Render/Shader/AtomShaders.cmake`；引擎只维护内置 shader 默认包。
- [ ] 工具负责 GLSL → SPIR-V，并按目标平台生成可选 DXIL/MSL 变体及 reflection metadata。
- [!] 本轮暂不实现编译器和外部项目 CMake 集成，先冻结资源包格式与公共 pipeline 描述。

### CORE-009：Utilities 目录职责与命名

- `Utilities/Utf8` 当前只负责 UTF-8 ↔ 宽字符转换，名称准确，暂不为了“看起来统一”改名。
- 如果后续扩展为 UTF-16、locale、路径规范化等完整编码服务，再整体迁移为 `Utilities/Encoding`，同时统一 target/API 和所有 include；不要只改目录名。
- `Utilities/Packager` 继续作为运行时/工具共用的资源打包模块；Utilities 按职责拆分，不建立一个集中式杂物目录。
- `atom-shaderc` 属于开发工具链，未来应放在 `Tools/Shaderc`（或独立工具仓库），不放进运行时 `Utilities`；它的输出是资源包，不是 Atom 核心库依赖。

### CORE-006：未实现模块的正式 API 管理

- [ ] Video 等空壳模块在实现前标为 Experimental，或不进入稳定公共 API。
- [ ] 每个公开模块至少具备最小能力、错误返回和示例。

### CORE-007：源码许可证标头统一

- [ ] 按照本文第 8 节的规则为 Atom 自有源码统一 MIT SPDX 标识。
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

### PRODUCT-003：动态链接发布配置

- [ ] 在公共 ABI、句柄所有权、异常/运行库策略稳定后，增加 `ATOM_BUILD_SHARED` 与 install/export 配置。
- [ ] 明确 Windows DLL、Linux SO、macOS dylib 的依赖部署、符号导出和版本策略。
- [ ] shader 资源保持独立包格式；C/C++ 动态链接不会改变 shader 的运行时加载方式。
- [!] 不把动态链接作为当前 SDL_GPU/RHI 重构前置条件，避免在 ABI 未冻结时引入部署和跨模块内存问题。


## 8. 源码许可证标头迁移（暂缓）

该任务只修改 Atom 自有 `.h/.hpp/.c/.cpp` 的注释，不修改 `ThirdParty/`：

- [!] 暂不批量删除 `All rights reserved`，保留现有文件头。
- [!] 暂不批量补充 `SPDX-License-Identifier: MIT`。
- [ ] 未来处理时使用真实作者和可确认的首次年份；无法确认时不凭空推测。
- [ ] 未来处理时保留 `@file`/`@brief` 等有价值说明，不强制 `@author`、`@date` 和空的 `@attention`。
- [ ] 作为独立机械提交完成，避免与功能改动混合。

新建 Atom 源文件和示例当前统一使用项目编码规范中的 Doxygen 文头模板（含
`@file/@author/@brief/@attention/@date` 与 `Copyright ... All rights reserved.`）。

当前审计结果：Atom 自有 C/C++ 文件 161 个，其中 45 个仍含旧标头，9 个已有 SPDX。可用下列命令复查：

```powershell
rg -l "All rights reserved" -g "*.h" -g "*.hpp" -g "*.c" -g "*.cpp" -g "!ThirdParty/**"
rg -L "SPDX-License-Identifier: MIT" -g "*.h" -g "*.hpp" -g "*.c" -g "*.cpp" -g "!ThirdParty/**"
```

## 9. 已接受的渲染基线

以下决策原先分散在渲染路线文档中，现集中维护于本清单：

1. 首个正式渲染后端是 SDL_GPU；由 SDL 根据平台和设备选择 D3D12、Vulkan 或 Metal，Atom 不复制一套底层 API 选择逻辑。
2. SDL_Renderer、SDL_Texture 和 `imgui_impl_sdlrenderer3` 不再作为 Atom 运行时后端；无 GPU 测试应使用未来的 Null/Recording Backend。
3. RHI 同时面向 2D 和基础 3D，公共头文件不得暴露 SDL_GPU、Vk、D3D 或 Metal 类型。Renderer2D/Renderer3D 只产生渲染意图，后端负责执行。
4. GLSL 是唯一人工维护的 shader 源语言，优先生成 SPIR-V；DXIL/MSL 是可选离线变体。shader 资源与 Atom C/C++ 链接方式无关。
5. 原生 Vulkan 是后续并列后端，必须复用同一套资源描述、shader 包和上层 Renderer；进程运行期间不支持后端热切换，GPU 资源不得跨 device 共享。
6. SDL_GPU 每帧顺序固定为：获取 command buffer → 获取 swapchain → 上传动态数据 → 3D → 2D/UI → Debugger → 提交。最小化或暂时没有 swapchain texture 时跳过该帧，不忙等。

## 10. 已关闭问题的回归记录

### MusicCard 封面区域异常

封面曾出现右下角小块或三角形裁剪，最终由两个独立问题叠加造成：

- `Vertex2D` 的 `FLOAT4` color 字段偏移不满足 D3D12 输入布局对齐；当前布局固定为 `color@0 / position@16 / uv@24`，stride 32。
- SDL_GPU 不接受 `SDL_SetGPUScissor(pass, nullptr)` 作为关闭裁剪；无 clip 时必须显式设置整帧 scissor，零面积交集直接跳过。

### MusicCard 首次切歌卡顿 / Shader 加载失败

同步打开大量音频曾耗尽 Windows C runtime 文件句柄，连带导致 shader 文件读取失败。当前约束是目录扫描只建立 Track stub，metadata/音频/封面按需加载，GPU 资源只在渲染线程创建；不得在后台线程直接调用 Renderer2D 或 SDL_GPU command buffer。

### 卡片后处理边界

Chromatic Aberration、Glitch 和 Gaussian Blur 必须只作用于卡片离屏目标；背景和 Debugger 直接写入交换链。全屏三角形使用 `(-1,-1)、(3,-1)、(-1,3)`，后处理区域通过 scissor 和圆角/羽化遮罩限制，避免重新引入全屏污染或矩形硬边。

## 11. 文档归档规则

- 本文件是 Atom 架构整改、渲染路线、许可证迁移和后续任务的唯一汇总入口。
- 具体模块 API 和使用方式仍放在模块 README（例如 `Render/Text/README-CN.md`、`Layout/README.md`）。
- 已完成任务的详细排查过程不再单独维护历史报告；如其中存在可复用的根因或验收命令，应提炼后写入本文件。

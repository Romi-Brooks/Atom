# Atom Engine API 使用指南

## 设计原则

- SDL3 等具体实现由 Atom 内部注册，普通用户不包含 `Backend/SDL3/*`。
- 音频播放后端全局选择，默认为 `AudioBackendId::Sdl3`；格式解码器由引擎默认注册（`.wav` → WavProf 首选 / SDL3Wav 兜底，`.mp3` → Minimp3Decoder）。游戏侧按路径加载，无需指定解码格式。
- `MusicPlayer`、`SFXPlayer`、`AudioMixer` 和 `MusicCrossfade` 仍是可自由组合的实例，不强制使用统一 `AudioSystem`。
- Backend 热切换会停止声音并清空 Player 中已注册的音频 ID；页面或后续场景需要重新 `Load/Play`。

## 日志

```cpp
#include <Log/LogSystem.hpp>

LOG_INFO(atom::log::core::Main, "Engine started");
LOG_WARNING(atom::log::core::Filesystem, "File not found");
LOG_ERROR(atom::log::core::Lua, "Script error");

atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);
```

## 窗口与 Screen

```cpp
#include <Window/RenderWindow.hpp>
#include <Runtime/ScreenManager.hpp>
#include <Runtime/Screen.hpp>

class MenuScreen final : public atom::Screen {
public:
    auto Render(atom::render::IRenderDevice& device) -> void override {
        device.Clear(atom::render::Color{30, 30, 60});
    }

    auto HandleEvent(const atom::window::IEvent&) -> bool override { return false; }
    auto Update(float delta_time) -> void override { /* game logic */ }
};

// ScreenManager 用 shared_ptr 持有 Screen，注册/切换/出栈都返回
// atom::ScreenOpResult，调用方可以区分「未注册」「同名冲突」「空指针」等结果。
const auto loaded = atom::ScreenManager::GetInstance().LoadScreen(
    "menu", std::make_unique<MenuScreen>());
if (loaded != atom::ScreenOpResult::Success) {
    // 可选值：Success / NotFound / NullScreen / InvalidName / NameConflict /
    //        Unregistered / InUse / EmptyStack / AlreadyVisible
    const auto reason = atom::ToString(loaded);
    LOG_ERROR(atom::log::core::ScreenManager, std::string{"LoadScreen failed: "} + std::string{reason});
    return 1;
}
// 按名字切换；需要裸指针时用 GetScreen() 取回（不要在 LoadScreen 之前缓存，
// 被拒绝的注册会立刻析构传入的 Screen）。
atom::ScreenManager::GetInstance().SwitchScreen("menu");
auto* menu = atom::ScreenManager::GetInstance().GetScreen("menu");

// 重新注册同名 Screen 必须显式声明策略：默认 Reject 返回 NameConflict；
// Replace 会先把 active/栈上的句柄重定向到新对象，再释放旧对象。
atom::ScreenManager::GetInstance().LoadScreen(
    "menu", std::make_unique<MenuScreen>(), atom::ScreenReplacePolicy::Replace);
// 卸载：仍为 active 时返回 InUse（先切走），其余情况连同引用它的栈槽一起移除。
atom::ScreenManager::GetInstance().UnloadScreen("menu");

auto& window = atom::RenderWindow::GetInstance();
window.Initialize("My Game", atom::algo::Vec2{1280, 720}); // 默认 RenderBackendId::SdlGpu
window.Initialize("My Game", atom::algo::Vec2{1280, 720},
                  atom::backend::RenderBackendId::SdlGpu); // 显式指定
window.SetFPS(60);
window.Run();
```

`Initialize` 的第三个参数选择**引擎内置**渲染后端（枚举 `atom::backend::RenderBackendId`），
默认 `SdlGpu`（内部注册表 ID `"sdl_gpu"`）。

### 渲染后端 vs 图形 API（两个正交的轴）

这两个概念必须分开，不能合成一个枚举：

| 轴 | 类型 | 回答的问题 | 何时新增成员 |
|---|---|---|---|
| 后端 | `RenderBackendId` | **哪份代码**实现 `IRenderBackend`（拥有 device、swapchain、管线、资源缓存） | 新写一个后端实现并产品化时 |
| 图形 API | `GpuDriver` | 这份后端**跑在哪个原生 API 上** | 几乎不变（D3D12 / Vulkan / Metal） |

所以：

- **SDL_GPU + Vulkan** = `RenderBackendId::SdlGpu` + `GpuDriver::Vulkan`（同一份代码换 API）
- **原子级 Vulkan 后端** = 未来的 `RenderBackendId::Vulkan` + `GpuDriver::Auto`（另一份代码）
- `GpuDriver::Auto` = 让后端自己挑（SDL_GPU 在 Windows 优先 D3D12，macOS 用 Metal，其余 Vulkan）

如果合成一个枚举，成员数会变成「后端 × 驱动」，而且再也回答不了"这份后端是谁实现的"。

### 显示与呈现（窗口模式 / 垂直同步）

```cpp
atom::runtime::ApplicationConfig config{};
config.title = "My Game";
config.resolution = atom::algo::Vec2{2560, 1440};
config.render_backend = atom::backend::RenderBackendId::SdlGpu;
config.gpu_driver = atom::backend::GpuDriver::Vulkan;          // 默认 Auto
config.window_mode = atom::window::WindowMode::Windowed;        // 默认 Windowed
config.present_mode = atom::render::PresentMode::VSync;        // 默认 VSync
application.Initialize(config);

// 运行期修改（这是游戏内"显示设置"页要调的两个选项）
window.SetWindowMode(atom::window::WindowMode::Borderless);
window.SetPresentMode(atom::render::PresentMode::Immediate);
```

| `PresentMode` | 提交是否阻塞 | 名义上是否跟随刷新率 | **实测（SDL_GPU / D3D12，RTX 4060）** | 典型游戏内文案 |
|---|---|---|---|---|
| `VSync` | 阻塞 | 是 | 窗口/无边框/独占 **全部 144** | 垂直同步：开 |
| `Mailbox` | 不阻塞（丢旧帧） | 是（Vulkan 语义） | 窗口/无边框 **≈2500（不限帧）**；独占 **144** | 垂直同步：开 + 三重缓冲 |
| `Immediate` | 不阻塞 | 否 | 窗口/无边框 **≈2500**；独占 **≈2000（撕裂）** | 垂直同步：关 |

**注意 Mailbox 这一行**：SDL_GPU 的 D3D12 路径把它实现成 `syncInterval=0` 且**不带** `ALLOW_TEARING`，
所以在窗口里它并不限速（合成器丢多余帧），只有在独占全屏下因为"不允许撕裂 → DXGI 强制 vblank 同步"才掉回刷新率。
Vulkan 路径才是真正的 mailbox（`VK_PRESENT_MODE_MAILBOX_KHR`，等 vblank）。
所以判断"现在到底有没有被限速"必须问设备：`IRenderDevice::IsPresentRefreshPaced()`，
不要用 `IsRefreshPaced(mode)` 这个纯函数（它只表达名义语义），更不要写 `mode != VSync`。

### 用计时拆解判断限速，不要用帧率推断

一个反例，值得记住：Vulkan 路径在四种窗口/呈现组合下都测到 143–151 fps，看起来像"Vulkan 被限制在刷新率"。
把 `FrameTiming` 拆开之后，同一帧是：

```
CPU record  6.637 ms      CPU submit  0.052 ms      GPU wait  0.002 ms
```

`submit` 和 `wait` 都是空的 —— **present 根本没有阻塞**。那个 144 是 6.9ms CPU 成本的倒数，恰好接近刷新率而已。

结论：**"帧率≈刷新率"不是限速的证据**。判断谁在限速只能看 `cpu_encode_ms` / `cpu_submit_ms` / `cpu_wait_ms`
三者各占多少（`Render` 面板已经直接给出判定）。任何"某个 API 被限制在刷新率"的结论，都必须先排除 CPU 成本。

| `WindowMode` | 语义 | 典型游戏内文案 |
|---|---|---|
| `Windowed` | 有边框、可缩放，使用 `config.resolution` | 窗口化 |
| `Borderless` | 无边框窗口，使用 `config.resolution`，居中；设成显示器分辨率就覆盖全屏 | 无边框窗口 |
| `ExclusiveFullscreen` | 真实切换显示模式，真正拿走显示输出 | 全屏（独占） |

**没有单独的"无边框全屏"模式**：无边框窗口 + 显示器分辨率就是那个状态。给它一个独立枚举值，
只会让调用方在同一个状态上做二选一，还要多维护一条代码路径。判断用
`IsFullscreen(mode)`（只有独占全屏为真）和 `IsUndecorated(mode)`（无边框/独占为真）。

三条硬约束：

1. **两个 setter 都延迟到帧边界生效**（`RenderWindow::BeginFrame`），因为 swapchain 不能在帧中途重建。
   已排队的请求被后端拒绝时只记一条警告，`GetWindowMode()` / `GetPresentMode()` 始终报告**实际生效值**。
2. **`Mailbox` 不是"不限帧"**。它只是不阻塞提交，仍然被刷新率限速；判断是否被限速请用
   `atom::render::IsRefreshPaced(mode)`，不要写 `mode != VSync`。
3. **`PresentMode` 是应用请求，不是驱动策略**。显卡控制面板的全局垂直同步（NVIDIA "Vertical sync"、
   AMD "Wait for vertical refresh"）可以覆盖它，引擎读不回来。所以 `Immediate` 只保证"请求已被接受"，
   不保证"一定不限帧"；若抓帧工具显示 `SyncInterval=1` 而应用请求的是 `Immediate`，是驱动或兼容层赢了。

`IsVSyncEnabled()` / `SetVSync(bool)` 保留为兼容层（true → `VSync`，false → `Immediate`）。
新代码不要再加二态选项：游戏内设置应当暴露三态，否则"关"到底指 `Mailbox` 还是 `Immediate` 永远说不清。

这套设置由 `Display/` 模块拥有（`DisplaySettings` 值对象 + `DisplaySystem` 单例），**不在 `RenderWindow` 上**：
swapchain 只能在帧边界重建，而设置页、配置文件、调试面板都想改它，状态放在窗口门面上会有多个所有者。

```cpp
#include <Display/DisplaySystem.hpp>

// 请求（排队，下一帧边界生效；重复请求同一值不会重建 swapchain）
atom::display::DisplaySystem::GetInstance().RequestWindowMode(atom::window::WindowMode::Borderless);
atom::display::DisplaySystem::GetInstance().RequestPresentMode(atom::render::PresentMode::Immediate);

// 读取实际生效值（被后端拒绝时保持原值，不会假装设置成功）
const auto applied = atom::display::DisplaySystem::GetInstance().Applied();
```

`Application::Run` 每帧在 `Window::BeginFrame()` 之前调一次 `ApplyPending()`，这就是唯一的应用点。

三个调试面板各答一个问题，互不重叠：

| 面板 | 回答 |
|---|---|
| `Debugger/Panels/FpsDebugger.hpp` | 多快：帧率、frame ms、min/avg/max、spike、曲线 |
| `Debugger/Panels/RenderDebugger.hpp` | 时间花在哪、跑在什么设备上：CPU submit / GPU wait、限速判定、backend/API/device/driver/version |
| `Debugger/Panels/DisplayDebugger.hpp` | 谁在限制它：窗口模式 + 呈现模式下拉、实际生效值、是否受刷新率限速 |

新后端由 Atom 引擎侧实现并在发版时加入枚举；游戏/Mod 代码不注册渲染后端。
具体后端实现（`Backend/SDL3/*` 等）由引擎运行时内部注册与持有；普通用户通过
`Window/*`、`Render/*`、`Media/*` 和 `Event/*` 编程，不直接包含具体后端头文件。

### 调试覆盖层（ImGui）

`Debugger/Overlay.hpp` 是写自定义调试面板的唯一入口：导出 ImGui +
`atom::debugger::DebugPanel` + 默认槽位工具。内置日志面板是
`atom::debugger::LogDebugger`（is-a `DebugPanel`），与自定义面板平级挂载。

```cpp
#include <Debugger/Overlay.hpp>
#include <Debugger/Panels/LogDebugger.hpp>

class MyPanel final : public atom::debugger::DebugPanel {
protected:
    auto OnDrawOverlay() -> void override {
        atom::debugger::ApplyStatusPanelSlot();
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
    }
};

MyPanel stats{};
atom::debugger::LogDebugger logs{};
stats.Attach(atom::RenderWindow::GetInstance());
logs.Attach(atom::RenderWindow::GetInstance());

// 显隐与生命周期分离：SetEnabled 只开关面板（X 关掉后可再开）；
// Detach 才取消订阅并丢弃日志缓冲。
logs.SetEnabled(false);
logs.SetEnabled(true);
```

- 用户**不要**直接 `#include <imgui.h>` 或任何 `Backend/*` 头文件；`Debugger/Overlay.hpp` 是唯一入口。
- 可选工具按需 include：`Debugger/Panels/LogDebugger.hpp`、`Debugger/FontLoader.hpp`。
- 调试面板默认位置由 `atom::debugger::Apply*PanelSlot()` 在首次绘制时写入；ImGui 不再读写 `imgui.ini`。
- 一个 `Runtime::Application` 由 Runtime 集成的 `OverlayManager` 维护一份 ImGui context/backend；多个 `DebugPanel` 可独立 attach。
- 每个面板应使用不同的 `ImGui::Begin()` 窗口名，并在 `Begin` 前调用合适的 `Apply*PanelSlot()`。
- `FontLoader` 是调试字体工具（不是面板、不是 Atom 业务字体 API）；业务文字走 Renderer2D。
- 这里的“多个窗口”指同一 ImGui context 内的多个 ImGui window；如果需要拖出为原生 SDL 窗口，还需要额外实现 multi-viewports。

### Runtime 工具观察器

事件、帧更新、Overlay、Resize、Shutdown 均可由 Runtime 的工具观察器订阅（ARCH-112），返回 RAII
`RuntimeConnection`，析构自动注销；多个观察器可并存，注销一个不影响其他。它们适合 Debugger、Profiler
等工具，**不应用来替代游戏系统调度**：

```cpp
auto& app = atom::runtime::Application::GetInstance();
auto conn = app.AddUpdateObserver([](float dt) { /* 工具采样 */ });
auto resizeConn = app.AddResizeObserver([](uint32_t w, uint32_t h) { /* 调整工具资源 */ });
// conn / resizeConn 析构时自动移除对应观察器
```

## 输入

引擎有**两条**读输入的路径，它们不能互相替代。选错的典型后果是：拿事件去表达「按住」，最后自己在成员变量里手写一套 bool 数组；或者拿轮询去表达一次性命令，结果长按连发。

| | 事件方式 `Screen::HandleEvent(IEvent)` | 轮询方式 `input::InputSystem` |
|---|---|---|
| 拿到的东西 | 一个事件、一次发生 | 一帧结算后的整体状态 |
| 适合 | 一次性命令，并且需要事件本身的载荷（修饰键快照、点击时的鼠标坐标） | 持续输入：按住状态、鼠标位移、滚轮累积、每帧边沿 |
| 不适合 | 「按住」「移动了多少」「这一帧累积了多少」——事件是点，没有时长 | 需要精确匹配「按下那一刻的修饰键」的场景 |
| 修饰键语义 | 事件里的快照，可精确匹配（`Ctrl+S` 且没按别的） | 按**读的时候**的按住状态判定，所以修饰键先按后按都能触发 |
| 能否拦截事件 | 能。返回 `true` 后不再分发给下层 Screen | 不能。状态是全局的 |
| 一帧读多次 | 不方便，事件只来一次 | 天然支持，多处读到的一致 |

判断标准：**一个命令由一条事件决定，就写在 `HandleEvent`；一帧要读多次、或者跟按住时长有关的，就写在 `Update`/`FixedUpdate` 里读 `InputSystem`。**

轮询方式由引擎主循环自动驱动，`Runtime::Application::Run()` 在事件轮询前调用一次 `BeginFrame()`，每个规范化事件先进入状态机再分发给 Screen，所以 **业务代码不要自己再调 `BeginFrame()`**——同一帧调两次会把真实的按下边沿提前结算成 `Held`。

```cpp
#include <Input/ActionMap.hpp>
#include <Input/InputSystem.hpp>

enum class Action { Jump, Pause };

atom::input::ActionMap<Action> actions;
actions.Bind({atom::input::Key::Space}, Action::Jump);
actions.Bind({atom::input::Key::Escape}, Action::Pause);

// Update() / FixedUpdate() 里读：
auto& input = atom::input::InputSystem::GetInstance();
if (input.IsKeyHeld(atom::input::Key::W)) { /* 按住移动 */ }
if (input.IsActionPressed(actions, Action::Jump)) { /* 这一帧刚好按下，只触发一次 */ }
if (input.IsMouseHeld(atom::input::MouseButton::Left)) { /* 拖动 */ }
const auto delta = input.GetMouseDelta();
```

窗口失焦时引擎会自动调用 `Reset()` 释放所有按键，避免方向键卡在按住状态（SDL 失焦后不再发 key-up）。鼠标坐标不在 `Reset()` 范围内：失焦时鼠标仍有真实位置，清零会让重新聚焦后的第一次移动算出虚构的 delta；需要丢弃基线时显式调用 `ResetMousePosition()`。

### UI 抢输入（聊天框、菜单、调试面板）

只要游戏里有会吃键盘的界面——聊天框、背包、菜单——就必须回答一个问题：这次按键是给 UI 的，还是给游戏的。引擎不替你做这个决定，但把事实告诉你。这套机制**跟 UI 框架无关**：游戏自己的 UI 直接调用，调试层的 ImGui 只是其中一个调用者，所以 Debug 构建和 Release 构建的语义完全一致。

```cpp
auto& input = atom::input::InputSystem::GetInstance();

// 顺序很重要：先处理本帧的 UI 命令（这里是"按 T 打开聊天框"），再发布捕获声明。
// 反过来写的话，打开聊天框的那一帧声明的仍是"键盘归游戏"，于是本帧的打字会被接受、
// WASD 还会多走一帧，甚至那次用来打开聊天框的 T 会被当成聊天内容再读一次。
if (!chat_open && input.IsActionPressed(actions, Action::OpenChat))
    chat_open = true;
input.SetUiCapture(/*mouse=*/atom::input::Claim::Release,
                   /*keyboard=*/chat_open ? atom::input::Claim::Own : atom::input::Claim::Release,
                   /*text_input=*/chat_open ? atom::input::Claim::Own : atom::input::Claim::Release,
                   "chat box");
```

顺序正确时，游戏自己的 UI **没有延迟**：打开聊天框的下一帧 `FixedUpdate` 就已经被挡住。只有调试覆盖层会晚一帧，因为它的标志来自 `ImGui::NewFrame()` 之后。

三个标志的粒度不同，用途也不同：

| 标志 | 判据 | 该挡什么 |
|---|---|---|
| `IsMouseCapturedByUi()` | 有面板正被拖动 / 鼠标在 UI 上 | 点击、拖拽类输入 |
| `IsKeyboardCapturedByUi()` | 某个 UI 持有键盘上下文 | **一次性命令**（快捷键、R、Space）。聊天框开着时敲 r，不该同时触发"重置视角" |
| `IsTextInputCapturedByUi()` | 正在编辑文本 | **持续输入**（按住 WASD 移动）。打字时角色必须停住，但仅仅是"点了下面板"不该冻住角色 |

`InputSystem` 本身**不做屏蔽**：`IsMouseHeld`、`IsKeyHeld` 始终如实回答物理设备的状态，是否让位由业务决定。这是刻意的——"点一下调试面板算不算点了一下游戏"是游戏策略，不是引擎策略。

```cpp
// 移动（持续输入）用窄粒度
if (!input.IsTextInputCapturedByUi()) {
    if (input.IsKeyHeld(atom::input::Key::W)) { /* 前进 */ }
}
// 一次性命令用粗粒度
if (!input.IsKeyboardCapturedByUi() && input.IsActionPressed(actions, Action::Jump)) { /* 跳 */ }
```

文本本身不走按键查询，走 `EventType::TextInput` 事件（在 `Screen::HandleEvent` 里读 `TextInputEvent`），这样任意键盘布局和输入法都能正常工作。

调试覆盖层用的是同一个入口：`OverlayManager` 每帧把 ImGui 的鼠标捕获状态写进去（来源名 `ImGui debug overlay`），所以拖动调试面板不会连带拖动场景。**注意它只声明鼠标**——调试面板没有文本输入，而 ImGui 的 `WantCaptureKeyboard` 语义是"任何面板有焦点"，拿它当"游戏 UI 持有键盘"会让所有 Release 构建里根本不存在的调试层影响输入语义。

完整可运行样例：`Example/Input/InputDemo.cpp`（target `Example_Input_Demo`）——WASD/方向键按住移动方块、鼠标拖拽、Space/R/Escape 走 ActionMap，按 T 打开一个**游戏内的聊天框**（用 Renderer2D 画的），打开后 R/Space 一律不生效、打字时 WASD 停住、Enter 发送、Escape 关闭。面板实时打印按键状态、鼠标增量与三个捕获标志。

### 游戏 UI（`Atom_UI`）

正式游戏 UI 使用保留模式的 `UiTree`，不是 ImGui，也不把每一个按钮做成 ECS entity。`UiTree` 只负责页面内部的控件树、Yoga 布局、逆绘制顺序的命中测试、焦点与输入捕获；`ScreenManager` 仍负责菜单/设置/游戏/暂停这些页面级导航。

```cpp
#include <UI/UiCanvas.hpp>
#include <UI/UiTree.hpp>

// Screen 持有它，而不是 Runtime 或 Window 持有它。
atom::ui::UiTree ui;
const auto button = ui.CreateButton(ui.Root(), "Start", button_layout, button_style, [] {
    atom::ScreenManager::GetInstance().SwitchScreen("game");
});

auto HandleEvent(const atom::window::IEvent& event) -> bool override {
    return ui.HandleEvent(event);       // true 时阻止向下层 Screen 传播
}
auto Update(float) -> void override {
    ui.Update();                        // 在固定更新之后释放本帧临时 capture
}
auto Render(atom::render::IRenderDevice& device) -> void override {
    const auto output = device.GetOutputSize();
    ui.Layout(output.GetX(), output.GetY());
    atom::ui::UiCanvas::RenderFrame(renderer, ui, font);
}
```

`UiCanvas` 总以 `(origin=0,0, zoom=1)` 提交，UI 坐标因此不受世界相机影响；若同一 device frame 已绘制世界，可在它之后提交 UI canvas。`ui.SetModal(true)` 会阻断未命中控件的鼠标/键盘事件，适合暂停层；`ScreenManager` 对 Screen 栈本身也按视觉最上层到最下层分发事件。完整的页面流参考 `Example/UI/UiRuntimeDemo.cpp`（target `Example_UI_Runtime_Demo`）：菜单 `SwitchScreen` 到设置或游戏，游戏 `PushScreen` 暂停页，暂停页 `PopScreen` 回游戏或 `SwitchScreen` 回菜单。

## Renderer2D 与图片

`Renderer2D` 通过薄的 `IRender2DContext` 使用当前设备，业务代码不接触 SDL_GPU
句柄。调用顺序必须位于设备帧之内：

```cpp
atom::render::Renderer2D renderer;
renderer.Initialize(device, ATOM_SHADER_OUTPUT_DIR);

device.BeginFrame();
device.Clear(atom::render::Color{16, 18, 24});
renderer.BeginFrame(camera_x, camera_y, zoom);
renderer.DrawRect({20, 20, 160, 80}, atom::render::Color{80, 160, 220});
renderer.DrawTexture(*texture, {220, 20, 128, 128});
renderer.EndFrame();
device.EndFrame();
```

图片解码与上传是两个独立步骤：`DecodeImageMemory/File` 返回后端无关 RGBA8，
再交给 `Renderer2D::CreateTexture`。stb_image 当前支持 PNG/JPEG/BMP/GIF/TGA/PSD/
HDR/PIC/PNM，不支持 WebP。Windows 非 ASCII 资产路径优先经 VFS/文件流读入内存，
再调用 `DecodeImageMemory`。

纹理、字体与 Renderer2D 必须在创建它们的 RenderDevice 销毁前释放。使用
`Runtime::Application` 时，可把 `renderer.Shutdown()` 注册到 `AddShutdownObserver`；返回的
`RuntimeConnection` 必须存活到 `app.Run()` 结束。

## 音乐

普通用户不需要创建 Decoder Registry 或 SDL3 Backend：

```cpp
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Audio/Transitions/MusicCrossfade.hpp>

atom::AudioMixer mixer;
atom::MusicPlayer music{mixer};
atom::audio::MusicCrossfade crossfade{music};

music.Load("menu", "assets/menu.wav");
music.Load("game", "assets/game.mp3"); // .mp3 由 minimp3 解码
music.Play("menu");

// 在 Screen::Update(delta_time) 中调用。
crossfade.Switch("game", 2.0f);
crossfade.Update(delta_time);
```

### Seek / 播放进度

```cpp
const float duration = music.GetDuration("menu"); // 秒；0 表示解码器无法报告长度
if (music.IsSeekable("menu")) {
    music.Seek("menu", 42.0f);          // 越界会按 duration 钳制
}
const float position = music.GetPlayingOffset("menu");
```

调用链是分层的，Seek 能力由解码器决定，任何一层都不会假装能做到：

```text
MusicPlayer::Seek(id, seconds)                  // Atom 层：按 duration 钳制 + 结果上报
  └─ IAudioSource::SetPlayingOffset(seconds)    // 播放后端契约，返回 bool
       ├─ buffered 音源：直接移动播放游标
       │    （SDL3MixerSource 用 MIX_SetTrackPlaybackPosition；SDL3MusicSource 用 PCM 游标）
       └─ streaming 音源：停解码线程 → 清空环形缓冲/SDL 队列 → 解码器定位 → 重启
            └─ IAudioDecoder::SeekToFrame(frame) // 解码器契约，IsSeekable() 声明能力
                 ├─ WavProfDecoder ：纯字节偏移，逐样本精确
                 └─ Minimp3Decoder ：mp3dec_ex_seek，采样级精确
```

- 正在播放时 Seek 会丢掉已经排队的数据，可能听到一次很短的静音空隙；停止/暂停状态下 Seek 只设置位置，下一次 `Play()` 从该位置开始。
- `SetPlayingOffset(0)` 永远可用（等价于 `Rewind()`）；非 0 目标在解码器 `IsSeekable() == false` 时返回 `false`，不会静默忽略。
- 自研解码器要实现 Seek，只需覆写 `IAudioDecoder::SeekToFrame(frame)` 与 `IsSeekable()`；播放层不需要任何改动。
- MP3 的长度与帧索引在 `Open()` 时由 minimp3 扫描得到（VBR 标签存在时直接采用标签值），因此 `GetDuration` 对 MP3 通常可用。

### 资源包 + 内存流式播放

`MusicPlayer::LoadFromMemory` 可以直接从内存缓冲流式播放（例如
`Unpackager::ExtractFileToMemory` 从资源包读出的内容），全程不写临时文件。
解码器（minimp3 / WavProf / SDL3Wav）均实现了 `IAudioDecoder::OpenFromMemory`：

```cpp
#include <Packager.hpp>
#include <Unpackager.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>

// 1. 打包（也可用 packager_tool.exe，或只跑 2/3 步读取现成资源包）
atom::tools::Packager packer;
atom::tools::Packager::Config config;
config.verbose = true;
config.preserveStructure = false; // 条目仅用文件名（保留扩展名，便于按扩展名选解码器）
packer.Pack({R"(E:\Music\demo.mp3)", R"(E:\Music\demo.wav)"}, "music.pak", config);

// 2. 打开资源包并把每个条目完整读入内存
atom::tools::Unpackager unpacker;
unpacker.Load("music.pak");
std::vector<atom::tools::Unpackager::MemoryFile> files;
unpacker.ExtractAllToMemory(files);

// 3. 直接从内存缓冲流式播放。
//    注意：缓冲是借用的，调用方必须保证它比轨道存活得更久（此处 files
//    与 music 同生命周期）。
for (std::size_t i = 0; i < files.size(); ++i) {
    music.LoadFromMemory("track_" + std::to_string(i),
                         files[i].filename, files[i].GetData(), files[i].GetSize());
}
music.Play("track_0");
```

完整可运行示例见 `Example/Media/PackagedMusicPlayback.cpp`（目标 `Example_Packaged_Music`）：
首次运行自动把 E:\Music 下的样例打成 `music_demo.pak`（删除该文件即可重新打包）。

## 音效

```cpp
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/SFXPlayer.hpp>
#include <Media/Audio/Resources/AudioClipCache.hpp>

atom::AudioMixer mixer;
atom::AudioClipCache clips;
atom::SFXPlayer sfx{clips, mixer};

sfx.Load("explosion", "assets/explosion.wav");
sfx.Play("explosion");
sfx.Play("explosion"); // VoicePool 允许重叠播放
```

## 音量

```cpp
atom::AudioMixer mixer;
mixer.SetMasterVolume(80.0f);
mixer.SetMusicVolume(70.0f);
mixer.SetSFXVolume(100.0f);
```

当前 `AudioMixer` 是轻量分类音量配置。完整 Bus 增益传播仍属于后续事项。

## 音频元数据

读取音频文件的标签与属性（基于 TagLib，由引擎封装，调用方无需包含
TagLib 头）：

```cpp
#include <Media/Audio/Metadata/AudioMetadataReader.hpp>

if (const auto meta = atom::AudioMetadataReader::Read("assets/menu.mp3")) {
    std::cout << meta->title << " - " << meta->artist << std::endl;
    std::cout << meta->durationSeconds << "s, " << meta->sampleRate
              << " Hz, " << meta->channels << " ch" << std::endl;
} else {
    // 文件无法读取或没有标签
}
```

`AudioMetadata` 字段：`title/artist/album/comment/genre/year/track` +
`durationSeconds/bitrateKbps/sampleRate/channels`（属性不可用时为 0）。

## 全局 Backend 设置

默认设置：

```text
audio backend = sdl3
```

游戏设置页面可以这样切换：

```cpp
#include <Backend/Composition/BackendRuntime.hpp>
#include <Backend/Contracts/Audio/AudioBackendId.hpp>

auto& backends = atom::backend::BackendRuntime::GetInstance();

if (!backends.SetAudioBackend(atom::backend::AudioBackendId::Sdl3Mixer)) {
    // 当前内置后端初始化失败——已激活的后端与 source 不受影响。
}
```

切换规则：

1. 先创建新后端实例；创建失败即返回 `false`，当前后端与它拥有的 source 不受任何影响。
2. 通知所有接入全局 Runtime 的 Music/SFX Player（`OnAudioBackendChanging`）：停止声音并清空所有已注册 ID、Source、VoicePool 和缓存。自建缓存（例如音乐卡片自己记录的"已加载"标记）也应在这里失效。
3. 调用旧后端的 `Quiesce()`，把它创建的其余 source 一并失效（`IAudioSource::Detach()`）。
4. 替换后端、generation +1，再通知 `OnAudioBackendChanged()`；当前页面或后续场景重新执行 `Load/Play`。

切换后 ID 不会自动恢复，播放位置也不会迁移。需要判断缓存是否作废时使用 generation：

```cpp
const auto generation = backends.GetAudioBackendGeneration();
// ... 异步加载 ...
if (backends.GetAudioBackendGeneration() != generation) {
    // 这份结果是上一个后端产生的，丢弃并重试。
}
```

访问当前后端：

```cpp
backends.Audio();                 // IAudioBackend&，无活动后端时返回 NullAudioBackend，不抛异常
backends.TryAudio();              // IAudioBackend*，无活动后端时为 nullptr
backends.AcquireAudioBackend();   // std::shared_ptr<IAudioBackend>，创建 source 时使用
```

Beta 阶段建议只在主菜单或设置页面切换，并且从主线程发起。游戏运行状态检测与禁止策略将在后续实现。

## 自定义 Backend（高级用法）

普通项目不需要操作 Registry。自定义后端开发者可以注册播放后端工厂：

```cpp
auto& runtime = atom::backend::BackendRuntime::GetInstance();

runtime.Registry().RegisterAudioBackend("custom", [] {
    return std::make_unique<MyAudioBackend>();
});
```

自定义格式解码器直接注册到解码器注册表：

```cpp
auto& decoders = atom::backend::BackendRuntime::GetInstance().AudioDecoders();
decoders.Register(".ogg", [] { return std::make_unique<MyOggDecoder>(); }, "MyOgg");
```

`Register` 决定首选实现，`RegisterFallback` 追加兜底候选（只有前一个候选**声明**无法处理该文件时才继续），`Replace` 用单个解码器替换整条链。候选的职责边界：解码器只负责"我能不能解 + 不能解的原因"（`DecoderOpenStatus`），尝试顺序属于注册策略，尝试循环与日志属于 `AudioClipLoader`。

`.wav` 默认就是一条链：自研 `WavProf`（流式、内存恒定、无压缩格式）优先，SDL3 的 `SDL_LoadWAV` 兜底（覆盖 ADPCM / A-Law / µ-Law，但会把整段 PCM 驻留内存）。要强制单实现：

```cpp
decoders.Replace(".wav", atom::backend::audio_decoder::CreateSDL3WavDecoder, "SDL3Wav");  // 只用 SDL3
decoders.Replace(".wav", atom::backend::audio_decoder::CreateWavProfDecoder, "WavProf");  // 只用自研
```

测试或特殊工具仍可使用显式注入构造，不受全局切换影响：

```cpp
atom::audio::AudioDecoderRegistry test_decoders;
atom::backend::BackendRuntime::RegisterDefaultAudioDecoders(test_decoders);
atom::MusicPlayer music{fake_backend, test_decoders, mixer};
atom::SFXPlayer sfx{fake_backend, clips, mixer};
```

## Lua

Lua 继续操作注入的 Player：

```cpp
SetLuaMusicInstance(music);
SetLuaSFXInstance(sfx);
SetLuaAudioMixerInstance(mixer);
SetLuaMusicCrossfadeInstance(crossfade);
```

Backend 切换后，Lua 页面同样需要重新调用 `Music:Load`/`SFX:Load`，再继续播放。

## ECS（实体与组件）

`ECS` 只负责**存储**：持有实体和组件数据，不持有行为。系统（物理、动画、AI）属于各自的子系统，由它们遍历这里的组件。物理侧的分工见 `Physics/README.md`。

### 边界规则：公开头里没有第三方类型

组件存储后端是 `EntityWorld.cpp` 的实现细节。任何公开头文件——包括本模块自己的——都不出现 `entt::` 名字。这条规则换来三件事：后端可替换；`Entity` 的世代宽度由引擎决定（默认打包句柄只有 12 位版本，4096 次回收就回绕，旧句柄会「复活」）；Lua 绑定层只认引擎自己的 `Entity`。

### 创建与实体生命周期

```cpp
#include <ECS/Components.hpp>
#include <ECS/EntityWorld.hpp>

auto created = atom::ecs::EntityWorld::Create();
if (!created) {
    LOG_ERROR(channel, created.error().Message());
    return;
}
auto world = std::move(*created);

auto player = world->CreateEntity();          // 句柄：32 位索引 + 32 位世代
if (!world->IsAlive(player)) { /* 索引耗尽，已记日志 */ }

const auto destroyed = world->Destroy(player);
if (!destroyed)                               // 重复销毁会报 EcsEntityNotAlive
    LOG_WARN(channel, destroyed.error().Message());
```

销毁会释放槽位；该槽位再次分配时世代 +1，因此**旧句柄自动失效**，不会误指新实体。

### 组件读写

```cpp
// 加组件：实体已拥有该组件时失败，不会静默覆盖旧值
if (auto added = world->Emplace<Position>(player, Position{{0.0f, 0.0f}}); !added)
    LOG_ERROR(channel, added.error().Message());

// 明确表示「替换」时用这个
world->EmplaceOrReplace<Position>(player, Position{{1.0f, 2.0f}});

// 读：返回 nullptr 表示实体已死或没有该组件，不会崩
if (auto* position = world->TryGet<Position>(player)) { /* use *position */ }

// 需要区分「实体没了」和「组件不在」时用 Get
auto position = world->Get<Position>(player);
if (!position)
    LOG_ERROR(channel, position.error().Message());   // 两种原因各自的 ErrorCode

world->Patch<Position>(player, Position{{5.0f, 6.0f}});   // 组件必须已存在
world->Remove<Position>(player);                          // 幂等
```

### 遍历：ForEach

系统的主要写法是在后端自己的遍历过程里回调，零分配、零逐实体查表：

```cpp
struct Integrate {
    static auto Run(void* context, atom::ecs::Entity entity,
                    std::span<atom::ecs::Position> positions,
                    std::span<atom::ecs::Velocity> velocities) -> void {
        auto& dt = *static_cast<float*>(context);
        positions[0].value = positions[0].value + velocities[0].value * dt;
    }
};

float dt = 1.0f / 120.0f;
world->ForEach<atom::ecs::Position, atom::ecs::Velocity>({&Integrate::Run, &dt});
```

要点：

- 每个组件以**长度 1 的 `std::span`** 传入，所以循环体里写 `positions[0]`。
- 访问器是**函数指针 + context**，不是 lambda。因为 `EntityWorld.hpp` 不能包含后端，循环体必须在能看到后端的翻译单元里编译，函数指针是「无后端的头文件」能承载的东西。**参数仍是真实类型，调用点没有 `void*` 转换。**
- **标签不能列进 `ForEach`**：后端回调会省略空类型（没有值可交），签名永远无法满足。改为在循环体里逐实体查：`if (world->Has<Active>(entity)) { ... }`。
- 遍历顺序是后端池顺序，**对同一序列的世界操作稳定**——定步长模拟需要的正是这个性质。
- 组件组合需要显式实例化；未实例化是**链接错误**（不是静默空循环）。加组合见 `ECS/README.md`。

### 需要实体列表时：With

```cpp
// 按索引升序返回，代价是分配 + 排序（比 ForEach 慢约 3 倍）
for (const auto entity : world->With<atom::ecs::Position, atom::ecs::Velocity>()) { ... }
```

延迟销毁的扫描模式用它——先收集再销毁，系统就不会在自己遍历的过程中改动存储：

```cpp
world->Emplace<atom::ecs::PendingDestroy>(entity);      // 标记
const auto doomed = world->With<atom::ecs::PendingDestroy>();
for (const auto entity : doomed)
    world->Destroy(entity);                             // 遍历结束后才销毁
```

### 标签与编译期拒绝

空类型就是标签（`Active`、`Static`、`PendingDestroy`）：存在即全部信息。

以下误用是**编译错误**，不是运行期错误：对标签调用 `TryGet`/`Get`/`Patch`/`EmplaceOrReplace`（标签没有值），以及把标签列进 `ForEach`。

### 加一个新组件

1. 在 `ECS/Components.hpp` 定义（纯数据：无虚函数、无行为、不反向持有 world）。
2. 加进同文件的 `ATOM_ECS_COMPONENTS` 清单。
3. 在 `EntityWorld.cpp` 加一行 `ATOM_ECS_INSTANTIATE(T)`；空标签类型用 `ATOM_ECS_INSTANTIATE_TAG(T)`。
4. 若有系统要把它和其他组件一起遍历，再加一行 `ATOM_ECS_INSTANTIATE_ITERATION(...)`；只用于扫描（如标签）的组合用 `ATOM_ECS_INSTANTIATE_COLLECTION(...)`。

可运行示例：`Example/ECS/EntityWorldTour.cpp`（无窗口控制台导览，CMake 目标 `Example_ECS_Tour`）。

## 快速参考

| 类型 | 默认使用方式 | 说明 |
|---|---|---|
| `BackendRuntime` | `GetInstance()` | 全局 Backend 选择与热切换 |
| `RenderWindow` | `GetInstance()` + `Initialize(title, size, RenderBackendId)` | 窗口和主循环门面；默认 `RenderBackendId::SdlGpu` |
| `ScreenManager` | `GetInstance()` | Screen 注册、切换与调度 |
| `InputSystem` | `InputSystem::GetInstance()` | 按帧输入状态（按住/边沿/鼠标/UI 捕获）；`HandleEvent` 与它的分工见「输入」一节 |
| `EntityWorld` | `EntityWorld::Create()` + 移动构造 | 实体与组件存储；生命周期、组件读写、`ForEach`/`With` 遍历 |
| `AudioMixer` | 普通实例 | Master/Music/SFX 分类音量 |
| `MusicPlayer` | `MusicPlayer(mixer)` | 默认使用全局音频后端与引擎默认解码器 |
| `AudioClipCache` | 默认构造 | 默认使用全局解码器注册表 |
| `SFXPlayer` | `SFXPlayer(clips, mixer)` | 默认使用全局音频后端 |
| `MusicCrossfade` | `MusicCrossfade(music)` | 帧驱动音乐过渡 |
| `AudioMetadataReader` | `AudioMetadataReader::Read(path)` | 读取音频标签与属性（基于 TagLib） |
| `DebugPanel` | 继承 + `Attach` + 可选 `LogDebugger` | ImGui 调试面板基类（include `<Debugger/Overlay.hpp>`） |

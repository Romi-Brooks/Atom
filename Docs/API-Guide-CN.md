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
#include <Window/ScreenManager.hpp>
#include <Window/Screen.hpp>

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
默认 `SdlGpu`（内部注册表 ID `"sdl_gpu"`）。SDL_GPU 根据平台、驱动和本次构建提供的
Shader 格式选择 D3D12、Vulkan 或 Metal；Atom 上层不直接选择这些原生 API。
新后端由 Atom 引擎侧实现并在发版时加入枚举；游戏/Mod 代码不注册渲染后端。

具体后端实现（`Backend/SDL3/*` 等）由引擎运行时内部注册与持有；普通用户通过
`Window/*`、`Render/*`、`Media/*` 和 `Event/*` 编程，不直接包含具体后端头文件。

### 调试覆盖层（ImGui）

`Debugger/Overlay.hpp` 是写自定义调试面板的唯一入口：导出 ImGui +
`atom::debugger::DebugPanel` + 默认槽位工具。内置日志面板是
`atom::debugger::LogDebugger`（is-a `DebugPanel`），与自定义面板平级挂载。

```cpp
#include <Debugger/Overlay.hpp>
#include <Debugger/LogDebugger.hpp>

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
- 可选工具按需 include：`Debugger/LogDebugger.hpp`、`Debugger/FontLoader.hpp`。
- 调试面板默认位置由 `atom::debugger::Apply*PanelSlot()` 在首次绘制时写入；ImGui 不再读写 `imgui.ini`。
- 一个 `RenderWindow` 由窗口级 `OverlayManager` 维护一份 ImGui context/backend；多个 `DebugPanel` 可独立 attach。
- 每个面板应使用不同的 `ImGui::Begin()` 窗口名，并在 `Begin` 前调用合适的 `Apply*PanelSlot()`。
- `FontLoader` 是调试字体工具（不是面板、不是 Atom 业务字体 API）；业务文字走 Renderer2D。
- 这里的“多个窗口”指同一 ImGui context 内的多个 ImGui window；如果需要拖出为原生 SDL 窗口，还需要额外实现 multi-viewports。

### 窗口扩展监听器

事件、帧更新、Overlay、Resize、Shutdown 均通过监听器注册（ARCH-112），返回 RAII
`ListenerConnection`，析构自动注销；多个监听器可并存，注销一个不影响其他：

```cpp
auto conn = window.AddUpdateListener([](float dt) { /* 每帧更新 */ });
auto resizeConn = window.AddResizeListener([](uint32_t w, uint32_t h) { /* 窗口尺寸变化 */ });
// conn / resizeConn 析构时自动移除对应监听器
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

轮询方式由引擎主循环自动驱动，`RenderWindow::Run()` 在事件轮询前调用一次 `BeginFrame()`，每个规范化事件先进入状态机再分发给 Screen，所以 **业务代码不要自己再调 `BeginFrame()`**——同一帧调两次会把真实的按下边沿提前结算成 `Held`。

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
input.SetUiCapture(/*mouse=*/false, /*keyboard=*/chat_open, /*text_input=*/chat_open, "chat box");
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
`RenderWindow` 时，可把 `renderer.Shutdown()` 注册到 `AddShutdownListener`；返回的
`ListenerConnection` 必须存活到 `window.Run()` 结束。

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
#include <Backend/Runtime/BackendRuntime.hpp>
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

## 快速参考

| 类型 | 默认使用方式 | 说明 |
|---|---|---|
| `BackendRuntime` | `GetInstance()` | 全局 Backend 选择与热切换 |
| `RenderWindow` | `GetInstance()` + `Initialize(title, size, RenderBackendId)` | 窗口和主循环门面；默认 `RenderBackendId::SdlGpu` |
| `ScreenManager` | `GetInstance()` | Screen 注册、切换与调度 |
| `InputSystem` | `InputSystem::GetInstance()` | 按帧输入状态（按住/边沿/鼠标/UI 捕获）；`HandleEvent` 与它的分工见「输入」一节 |
| `AudioMixer` | 普通实例 | Master/Music/SFX 分类音量 |
| `MusicPlayer` | `MusicPlayer(mixer)` | 默认使用全局音频后端与引擎默认解码器 |
| `AudioClipCache` | 默认构造 | 默认使用全局解码器注册表 |
| `SFXPlayer` | `SFXPlayer(clips, mixer)` | 默认使用全局音频后端 |
| `MusicCrossfade` | `MusicCrossfade(music)` | 帧驱动音乐过渡 |
| `AudioMetadataReader` | `AudioMetadataReader::Read(path)` | 读取音频标签与属性（基于 TagLib） |
| `DebugPanel` | 继承 + `Attach` + 可选 `LogDebugger` | ImGui 调试面板基类（include `<Debugger/Overlay.hpp>`） |

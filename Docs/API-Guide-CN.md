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

// LoadScreen 返回非拥有指针（[[nodiscard]]），SwitchScreen 可直接用它
auto* menu = atom::ScreenManager::GetInstance().LoadScreen(
    "menu", std::make_unique<MenuScreen>());
atom::ScreenManager::GetInstance().SwitchScreen(menu);

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
        ImGui::Text("FPS: %.1f", GetFPS());
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
| `AudioMixer` | 普通实例 | Master/Music/SFX 分类音量 |
| `MusicPlayer` | `MusicPlayer(mixer)` | 默认使用全局音频后端与引擎默认解码器 |
| `AudioClipCache` | 默认构造 | 默认使用全局解码器注册表 |
| `SFXPlayer` | `SFXPlayer(clips, mixer)` | 默认使用全局音频后端 |
| `MusicCrossfade` | `MusicCrossfade(music)` | 帧驱动音乐过渡 |
| `AudioMetadataReader` | `AudioMetadataReader::Read(path)` | 读取音频标签与属性（基于 TagLib） |
| `DebugPanel` | 继承 + `Attach` + 可选 `LogDebugger` | ImGui 调试面板基类（include `<Debugger/Overlay.hpp>`） |

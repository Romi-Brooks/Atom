# Atom Engine API 使用指南

## 设计原则

- SDL3 等具体实现由 Atom 内部注册，普通用户不包含 `Backend/SDL3/*`。
- 音频播放后端全局选择，默认为 `sdl3`；格式解码器由引擎默认注册（`.wav` → WavProfDecoder，`.mp3` → Minimp3Decoder）。
- `MusicPlayer`、`SFXPlayer`、`AudioMixer` 和 `MusicCrossfade` 仍是可自由组合的实例，不强制使用统一 `AudioSystem`。
- Backend 热切换会停止声音并清空 Player 中已注册的音频 ID；页面或后续场景需要重新 `Load/Play`。

## 日志

```cpp
#include <Log/LogSystem.hpp>

LOG_INFO(atom::core::LogChannel::MAIN, "Engine started");
LOG_WARNING(atom::core::LogChannel::FILESYSTEM, "File not found");
LOG_ERROR(atom::core::LogChannel::LUA, "Script error");

atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);
```

## 窗口与 Screen

```cpp
#include <Window/RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Screen.hpp>

class MenuScreen final : public atom::Screen {
public:
    auto Render(atom::render::IRenderDevice& device) -> void override {
        device.Clear(atom::render::Color{30, 30, 60});
    }

    auto HandleEvent(const atom::window::IEvent&) -> bool override { return false; }
    auto Update(float delta_time) -> void override { /* game logic */ }
};

atom::ScreenManager::GetInstance().LoadScreen(
    "menu", std::make_unique<MenuScreen>());
atom::ScreenManager::GetInstance().SwitchScreen("menu");

auto& window = atom::RenderWindow::GetInstance();
window.Initialize("My Game", atom::algo::Vec2{1280, 720});           // 默认渲染后端 "sdl_gpu"
window.Initialize("My Game", atom::algo::Vec2{1280, 720}, "sdl_gpu"); // 显式指定后端
window.SetFPS(60);
window.Run();
```

`Initialize` 的第三个参数（`backendId`）选择渲染后端，默认 `"sdl_gpu"`。SDL_GPU
根据平台、驱动和本次构建提供的 Shader 格式选择 D3D12、Vulkan 或 Metal；Atom
上层不直接选择这些原生 API。自定义后端通过 `RenderBackendRegistry` 注册后即可选用：

```cpp
#include <Backend/Registry/RenderBackendRegistry.hpp>

atom::backend::RenderBackendRegistry::GetInstance().RegisterBackendFactory(
    "my_backend", [] { return std::make_unique<MyRenderBackend>(); });
// window.Initialize("My Game", atom::algo::Vec2{1280, 720}, "my_backend");
```

具体后端实现（`Backend/SDL3/*` 等）由引擎运行时内部注册与持有，普通用户只使用
`Backend/Contracts/*` 接口，不直接包含后端头文件。

### 调试覆盖层（ImGui）

`Debugger` 的公共接口保持后端无关，运行时会为默认 `sdl_gpu` 后端选择
`imgui_impl_sdlgpu3`。用户只需要引擎头：

```cpp
#include <Window/Overlay.hpp> // 引擎导出 ImGui API + atom::Debugger

class MyDebugger final : public atom::Debugger {
protected:
    auto OnDrawOverlay() -> void override {
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", GetFPS());
        ImGui::End();
    }
};

MyDebugger debugger{};
debugger.Attach(atom::RenderWindow::GetInstance());
```

- 用户**不要**直接 `#include <imgui.h>` 或任何 `Backend/*` 头文件；`Overlay.hpp` 是唯一入口。
- 当前一个进程只允许一个活动的 SDL_GPU ImGui `Debugger`；第二次 `Attach()` 会失败并记录 ERROR。多个 ImGui 面板要等窗口级共享上下文（ARCH-113）；非 ImGui 扩展仍可通过监听器并存。

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

### 资源包 + 内存流式播放

`MusicPlayer::LoadFromMemory` 可以直接从内存缓冲流式播放（例如
`Unpackager::ExtractFileToMemory` 从资源包读出的内容），全程不写临时文件。
解码器（minimp3 / WavProf）均实现了 `IAudioDecoder::OpenFromMemory`：

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

auto& backends = atom::backend::BackendRuntime::GetInstance();

if (!backends.SetAudioBackend("sdl3")) {
    // 后端不存在、初始化失败，或旧后端恢复失败。
}
```

切换规则：

1. 通知所有接入全局 Runtime 的 Music/SFX Player。
2. Player 停止声音并清空所有已注册 ID、Source、VoicePool 和缓存。
3. 销毁旧播放后端并创建新后端。
4. 当前页面或后续场景重新执行 `Load/Play`。

Beta 阶段建议只在主菜单或设置页面切换。游戏运行状态检测与禁止策略将在后续实现。

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
decoders.Register(".ogg", [] { return std::make_unique<MyOggDecoder>(); });
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
| `RenderWindow` | `GetInstance()` + `Initialize(title, size, backendId)` | 窗口和主循环门面；默认渲染后端 `sdl_gpu` |
| `ScreenManager` | `GetInstance()` | Screen 注册、切换与调度 |
| `AudioMixer` | 普通实例 | Master/Music/SFX 分类音量 |
| `MusicPlayer` | `MusicPlayer(mixer)` | 默认使用全局音频后端与引擎默认解码器 |
| `AudioClipCache` | 默认构造 | 默认使用全局解码器注册表 |
| `SFXPlayer` | `SFXPlayer(clips, mixer)` | 默认使用全局音频后端 |
| `MusicCrossfade` | `MusicCrossfade(music)` | 帧驱动音乐过渡 |
| `AudioMetadataReader` | `AudioMetadataReader::Read(path)` | 读取音频标签与属性（基于 TagLib） |
| `Debugger` | 普通实例 + `Attach` | ImGui 调试覆盖层（include `<Window/Overlay.hpp>`；一个窗口建议一个 ImGui Debugger） |

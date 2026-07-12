# Atom Engine API 使用指南

---

## 设计原则

引擎组件分为两类：

**管理器（全局唯一，引擎内置）** — 直接通过静态方法访问：
- `Log` — 日志系统
- `RenderWindow` — 渲染窗口
- `ScreenManager` — 屏幕管理器
- `SFXManager` — 音频缓冲管理器
- `VolumeManager` — 全局音量管理
- `DecoderRegistry` — 解码器注册表（音频加载用）

**播放器/服务（用户按需创建）** — 直接构造实例：
- `SFX` — 音效播放（支持同一音效多实例重叠）
- `Music` — 音乐播放
- `MusicFade` — 音乐淡入淡出

---

## 日志系统

```cpp
#include <Log/LogSystem.hpp>

// 直接使用宏
LOG_INFO(atom::LogChannel::ATOM_MAIN, "Engine started");
LOG_WARNING(atom::LogChannel::ATOM_FILESYSTEM, "File not found");
LOG_ERROR(atom::LogChannel::ATOM_LUA, "Script error");
LOG_DEBUG(atom::LogChannel::SDL_BACKEND_AUDIO, "Stream opened: fmt=...");

// 设置日志显示级别（默认 INFO，不显示 DEBUG）
atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);  // 显示所有
atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_WARNING); // 仅 WARNING+ERROR

// 自定义日志通道
const atom::LogChannel GAME_NPC("Game.NPC");
LOG_INFO(GAME_NPC, "NPC spawned");

// 预定义频道：
// Atom.Audio.Music       — 音乐领域逻辑
// Atom.Audio.SFX         — 音效领域逻辑
// Atom.Audio.Plug.MusicFade — 淡入淡出插件
// SDL.Backend.Audio      — SDL 音频底层操作
// SDL.Backend.Video      — SDL 视频底层
// SDL.Backend.Render     — SDL 渲染底层
// SDL.Backend.Window     — SDL 窗口底层
```

---

## 窗口 + 屏幕管理

```cpp
#include <Window/RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Screen.hpp>

class MenuScreen : public atom::Screen {
    auto Render(atom::IRenderTarget& target) -> void override { /* ... */ }
    auto HandleEvent(const atom::IEvent& event) -> bool override { /* ... */ }
    auto Update(float deltaTime) -> void override { /* ... */ }
};

// ScreenManager 是全局单例
atom::ScreenManager::GetInstance().LoadScreen("menu", std::make_unique<MenuScreen>());
atom::ScreenManager::GetInstance().SwitchScreen("menu");

// RenderWindow 也是全局单例
auto& window = atom::RenderWindow::GetInstance();
window.Initialize("My Game", atom::Vec2{1280, 720});
window.Run();
```

---

## 音效（Voice Pool）

```cpp
#include <Media/Audio/SFX/SFX.hpp>

// SFX 是普通实例，可创建多个
atom::SFX sfx;
sfx.Load("explosion", "assets/explosion.wav");

// 同一音效可重叠播放（每个 Play 分配一个 voice，最多 8 个同时）
sfx.Play("explosion");   // voice 1
sfx.Play("explosion");   // voice 2 — 重叠播放，上一个不被切断
sfx.Play("explosion");   // voice 3
// 超出 8 个时自动覆盖最旧的 voice

// 播放时自动读取 VolumeManager 的有效音量
```

---

## 音乐 + 回调流式播放

```cpp
#include <Media/Audio/Music/Music.hpp>
#include <Media/Audio/Plugs/MusicFade.hpp>

atom::Music music;
music.Load("bgm", "assets/bgm.wav");
music.Play("bgm");       // 后台线程推送 PCM 到 SDL 音频流

// 音乐播放完成后自动停止；支持循环
auto* source = music.GetSound("bgm");
if (source) source->SetLooping(true);

// MusicFade 淡入淡出（需要 Music 实例引用）
atom::audio::MusicFade fade{music};
fade.Switch("bgm2", 2.0f);  // 2 秒内淡出→切换→淡入
```

---

## 解码器框架（IAudioDecoder + DecoderRegistry）

```cpp
#include <Engine/Audio/DecoderRegistry.hpp>

// 解码器自动按扩展名选择（.wav → AtomWavDecoderBackend）
auto decoder = DecoderRegistry::Create("music.wav");
decoder->Open("music.wav");

const auto& info = decoder->GetInfo();
// info.sample_rate, info.channels, info.bits_per_sample, info.is_float

// 读取 PCM 数据
std::vector<uint8_t> pcm(info.total_pcm_frames * info.channels * (info.bits_per_sample / 8));
decoder->DecodeChunk(pcm.data(), static_cast<uint32_t>(pcm.size()));
decoder->Close();
```

---

## 全局音量管理

```cpp
// VolumeManager 是全局单例
auto& vol = atom::VolumeManager::GetInstance();

vol.SetMasterVolume(80);       // 总音量（影响所有音频）
vol.SetSfxVolume(100);         // SFX 独立音量
vol.SetMusicVolume(80);        // Music 独立音量

// 实际播放音量 = 总音量 × 分类音量 / 100
// master=80, sfx=100  → 实际 80
// master=80, music=80 → 实际 64

// SFX 和 Music 播放时自动读取有效音量
```

---

## 调试覆盖层

```cpp
#include <Window/Debugger.hpp>

class MyDebugger : public atom::Debugger {
protected:
    auto OnDrawOverlay() -> void override {
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", GetFPS());
        ImGui::End();
    }
};

MyDebugger debugger;
debugger.Attach(window);    // 绑定到 RenderWindow
```

---

## Lua 脚本

```cpp
#include <Lua/LuaLoader.hpp>

atom::Music music;
atom::SFX sfx;

SetLuaMusicInstance(music);
SetLuaSFXInstance(sfx);

LuaLoader lua;
lua.Initialize();

// Lua 中调用：
// Music:Load("bgm", "bgm.wav")
// Music:Play("bgm")
// SFX:Load("exp", "exp.wav")
// SFX:Play("exp")
// VolumeManager.SetMasterVolume(80)
```

---

## 完整示例

```cpp
#include <Window/RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Media/Audio/Music/Music.hpp>
#include <Media/Audio/Plugs/MusicFade.hpp>

auto main() -> int {
    // 实例：用户按需创建
    atom::Music music;
    atom::audio::MusicFade fade{music};

    // 初始化音乐
    music.Load("bgm", "bgm.wav");
    music.Play("bgm");

    // 单例：直接使用
    atom::ScreenManager::GetInstance().LoadScreen("game", std::make_unique<GameScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("game");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("My Game", atom::Vec2{1280, 720});
    window.Run();
}
```

---

## 快速参考

| 类 | 访问方式 | 说明 |
|-----|---------|------|
| `Log` | `Log::GetLogInstance()` | 全局日志系统，默认 INFO 级别 |
| `RenderWindow` | `RenderWindow::GetInstance()` | 唯一的渲染窗口（SDL3） |
| `ScreenManager` | `ScreenManager::GetInstance()` | 全局屏幕管理，`Run()` 内部自动引用 |
| `SFXManager` | `SFXManager::GetManager()` | 全局音频缓冲缓存 |
| `VolumeManager` | `VolumeManager::GetInstance()` | 全局音量（总音量 + 分类音量） |
| `DecoderRegistry` | `DecoderRegistry::Create(path)` | 按扩展名创建解码器 |
| `SFX` | `SFX()` 构造实例 | 音效播放器，支持 8 个 voice 重叠 |
| `Music` | `Music()` 构造实例 | 音乐播放器，后台线程推送流式播放 |
| `MusicFade` | `MusicFade(Music&)` | 淡入淡出，需要 Music 引用 |
| `Debugger` | `Debugger()` + `Attach(window)` | 调试覆盖层（ImGui SDL3） |

---

## 日志频道参考

| 频道常量 | 显示名称 | 用途 |
|---------|---------|------|
| `ATOM_AUDIO_MUSIC` | `Atom.Audio.Music ->` | Music 加载/播放/停止 |
| `ATOM_AUDIO_SFX` | `Atom.Audio.SFX ->` | SFX 加载/播放/voice 池管理 |
| `ATOM_AUDIO_PLUG_MUSICFADE` | `Atom.Audio.Plug.MusicFade ->` | 淡入淡出过程 |
| `SDL_BACKEND_AUDIO` | `SDL.Backend.Audio ->` | SDL AudioStream 操作、解码器底层 |
| `SDL_BACKEND_VIDEO` | `SDL.Backend.Video ->` | SDL 视频操作 |
| `SDL_BACKEND_RENDER` | `SDL.Backend.Render ->` | SDL 渲染操作 |
| `SDL_BACKEND_WINDOW` | `SDL.Backend.Window ->` | SDL 窗口操作 |

# Atom Engine API 使用指南

## 设计原则

- SDL3 等具体实现由 Atom 内部注册，普通用户不包含 `Backend/SDL3/*`。
- 音频播放后端和 Decoder 后端分别全局选择，默认均为 `sdl3`。
- `MusicPlayer`、`SFXPlayer`、`AudioMixer` 和 `MusicCrossfade` 仍是可自由组合的实例，不强制使用统一 `AudioSystem`。
- Backend 热切换会停止声音并清空 Player 中已注册的音频 ID；页面或后续场景需要重新 `Load/Play`。

## 日志

```cpp
#include <Log/LogSystem.hpp>

LOG_INFO(atom::LogChannel::ATOM_MAIN, "Engine started");
LOG_WARNING(atom::LogChannel::ATOM_FILESYSTEM, "File not found");
LOG_ERROR(atom::LogChannel::ATOM_LUA, "Script error");

atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);
```

## 窗口与 Screen

```cpp
#include <Window/RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Screen.hpp>

class MenuScreen final : public atom::Screen {
public:
    auto Render(atom::IRenderTarget& target) -> void override {
        target.Clear(atom::Color{30, 30, 60});
    }

    auto HandleEvent(const atom::IEvent&) -> bool override { return false; }
    auto Update(float delta_time) -> void override { /* game logic */ }
};

atom::ScreenManager::GetInstance().LoadScreen(
    "menu", std::make_unique<MenuScreen>());
atom::ScreenManager::GetInstance().SwitchScreen("menu");

auto& window = atom::RenderWindow::GetInstance();
window.Initialize("My Game", atom::Vec2{1280, 720});
window.SetFPS(60);
window.Run();
```

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
music.Load("game", "assets/game.mp3"); // .mp3 默认由 minimp3 解码，与 Decoder 后端无关
music.Play("menu");

// 在 Screen::Update(delta_time) 中调用。
crossfade.Switch("game", 2.0f);
crossfade.Update(delta_time);
```

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

## 全局 Backend 设置

默认设置：

```text
audio backend         = sdl3
audio decoder backend = sdl3
```

游戏设置页面可以这样切换：

```cpp
#include <Backend/Runtime/BackendRuntime.hpp>

auto& backends = atom::BackendRuntime::GetInstance();

if (!backends.SetAudioBackend("sdl3")) {
    // 后端不存在、初始化失败，或旧后端恢复失败。
}

if (!backends.SetAudioDecoderBackend("builtin")) {
    // Decoder 后端不存在或注册失败。
}
```

切换规则：

1. 通知所有接入全局 Runtime 的 Music/SFX Player。
2. Player 停止声音并清空所有已注册 ID、Source、VoicePool 和缓存。
3. 销毁旧播放后端并创建新后端；Decoder 切换则替换全局 Decoder Registry 内容。
4. 当前页面或后续场景重新执行 `Load/Play`。

Beta 阶段建议只在主菜单或设置页面切换。游戏运行状态检测与禁止策略将在后续实现。

## 自定义 Backend（高级用法）

普通项目不需要操作 Registry。自定义后端开发者可以注册工厂：

```cpp
auto& runtime = atom::BackendRuntime::GetInstance();

runtime.Registry().RegisterAudioBackend("custom", [] {
    return std::make_unique<MyAudioBackend>();
});

runtime.Registry().RegisterAudioDecoderBackend(
    "custom-codecs",
    [](atom::AudioDecoderRegistry& registry) {
        return RegisterMyDecoders(registry);
    });
```

测试或特殊工具仍可使用显式注入构造，不受全局切换影响：

```cpp
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
| `RenderWindow` | `GetInstance()` | 窗口和主循环门面 |
| `ScreenManager` | `GetInstance()` | Screen 注册、切换与调度 |
| `AudioMixer` | 普通实例 | Master/Music/SFX 分类音量 |
| `MusicPlayer` | `MusicPlayer(mixer)` | 默认使用全局音频与 Decoder 后端 |
| `AudioClipCache` | 默认构造 | 默认使用全局 Decoder 后端 |
| `SFXPlayer` | `SFXPlayer(clips, mixer)` | 默认使用全局音频后端 |
| `MusicCrossfade` | `MusicCrossfade(music)` | 帧驱动音乐过渡 |
| `Debugger` | 普通实例 + `Attach` | ImGui 调试覆盖层 |

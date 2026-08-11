# Atom Backend Runtime 高阶扩展指南

## 适用范围

普通游戏项目不需要操作 `BackendRegistry`，也不应该包含 `Backend/SDL3/*`。默认构造的 `MusicPlayer`、`SFXPlayer` 和 `AudioClipCache` 会自动使用全局 `BackendRuntime`，播放与 Decoder 后端默认都是 SDL3。

本文面向实现新播放后端、新 Decoder 后端、Fake/Null 测试后端，或在设置页面提供全局后端选择的开发者。

## 两类音频后端

Playback Backend 与 Decoder Backend 必须独立：

```text
Decoder Backend  : WAV/OGG/MP3/FLAC → PCM
Playback Backend : PCM → Source/Stream → Audio Device
```

因此可以自由组合，例如 Builtin WAV Decoder + SDL3 Playback Backend，或者 FFmpeg Decoder + OpenAL Playback Backend。

## 注册自定义播放后端

```cpp
#include <Backend/Runtime/BackendRuntime.hpp>

auto& runtime = atom::BackendRuntime::GetInstance();

runtime.Registry().RegisterAudioBackend(
    "openal",
    []() -> std::unique_ptr<atom::IAudioBackend> {
        auto backend = std::make_unique<MyOpenALAudioBackend>();
        if (!backend->IsReady()) return nullptr;
        return backend;
    });
```

要求：

- ID 大小写不敏感，建议使用稳定的小写名称；
- Factory 每次调用都创建全新的 Backend 实例；
- 初始化失败返回 `nullptr`；
- Backend 析构释放设备、线程、Stream 和平台子系统租约；
- Source 不得在创建它的 Backend 销毁后继续存在。

## 注册自定义 Decoder 后端

```cpp
runtime.Registry().RegisterAudioDecoderBackend(
    "ffmpeg",
    [](atom::AudioDecoderRegistry& decoders) {
        bool result = true;
        result &= decoders.Register(".mp3", [] {
            return std::make_unique<FFmpegMp3Decoder>();
        });
        result &= decoders.Register(".ogg", [] {
            return std::make_unique<FFmpegOggDecoder>();
        });
        return result;
    });
```

Installer 接收一个空的临时 Registry。任一必要格式注册失败时应返回 `false`，Runtime 不会替换当前 Decoder Registry。

## 全局切换语义

```cpp
runtime.SetAudioBackend("openal");
runtime.SetAudioDecoderBackend("ffmpeg");
```

切换前，所有接入全局 Runtime 的 Player 会停止当前声音，删除全部注册 ID，并销毁 Music Source、SFX VoicePool 和缓存。系统不迁移播放位置，也不自动重播；切换后由页面或后续场景重新调用 `Load/Play`。

## 切换时机约束

Atom 不推荐在大量 ID 已注册时切换 Backend，例如正式 Gameplay、战斗或关卡运行期间。建议只在主菜单或“设置”页面切换，此时通常只有零到两个背景音乐/UI 音效 ID，清理和重新加载成本明确。

Gameplay 状态检测目前尚未由 Runtime 强制执行，项目应自行限制设置入口。Backend 切换应从主线程发起；当前 Listener Registry 不承诺与播放线程或资源加载线程并发切换时的线程安全。

## 显式注入

测试和独立工具可以绕过全局 Runtime：

```cpp
atom::MusicPlayer music{fake_backend, test_decoders, mixer};
atom::AudioClipCache clips{test_decoders};
atom::SFXPlayer sfx{fake_backend, clips, mixer};
```

显式注入的 Player 不注册全局切换监听器，也不会在全局 Backend 改变时自动清空。

## 当前内置 ID

```text
Audio Playback Backend:
- sdl3（默认）

Audio Decoder Backend:
- sdl3（默认）
- builtin（实验性 WAV RIFF Decoder）
```

只有一个播放后端时，重复设置 `sdl3` 不触发清理或重建。新增第二个播放后端后，同一套全局切换协议无需修改 Player。

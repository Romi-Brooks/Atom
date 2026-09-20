# Atom Backend Runtime 高阶扩展指南

## 适用范围

普通游戏项目不需要操作 `BackendRegistry`，也不应该包含 `Backend/SDL3/*`。默认构造的 `MusicPlayer`、`SFXPlayer` 和 `AudioClipCache` 会自动使用全局 `BackendRuntime`，播放后端默认是 SDL3（`sdl3`），格式解码器由引擎默认注册（`.wav` → WavProf 首选 + SDL3Wav 兜底，`.mp3` → Minimp3Decoder）。

本文面向实现新播放后端、Fake/Null 测试后端，或添加新音频格式解码器的开发者。

## 解码与播放的分层

```text
Decoder  : 文件格式 → PCM（AudioDecoderRegistry 按扩展名查找解码器）
Playback : PCM → Source/Stream → Audio Device（BackendRegistry 按 ID 查找后端）
```

解码器与播放后端相互独立：WAV/MP3 解码器可以搭配任何播放后端使用，新增格式只需注册一个 `IAudioDecoder`，无需改动播放链路。

## 注册自定义播放后端

```cpp
#include <Backend/Runtime/BackendRuntime.hpp>

auto& runtime = atom::backend::BackendRuntime::GetInstance();

runtime.Registry().RegisterAudioBackend(
    "openal",
    []() -> std::unique_ptr<atom::audio::IAudioBackend> {
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
- **只要 Backend 持有平台资源，就必须实现 `IAudioBackend::Quiesce()`**，见下节。

## Source 生命周期：Quiesce / Detach

播放后端拥有的往往是**进程级**资源：销毁一个 SDL3 音频后端会释放子系统租约并执行 `SDL_QuitSubSystem(SDL_INIT_AUDIO)`，而 SDL 会在这个过程中销毁进程内所有仍然存在的 `SDL_AudioStream`；SDL_mixer 侧则会连带销毁共享 mixer。因此"存活到后端析构之后"的 Source 不只是语义错误，而是悬垂指针。

引擎为此提供了统一的失效协议：

```cpp
// 后端的 source 工厂返回前，把 source 登记进自己的注册表。
auto source = std::make_unique<MySource>(...);
source->BindRegistry(sources_);      // sources_ 是 atom::audio::AudioSourceRegistry
return source;

// 后端实现 Quiesce()：销毁自己之前先让所有 source 失效。
auto MyBackend::Quiesce() -> void { sources_.DetachAll(); }
```

- Source 继承 `atom::audio::BackendOwnedSource`（它同时提供 `BindRegistry()` 和 `Detach()`），并实现唯一的钩子 `ReleaseBackendHandles()`：停止/join 线程、销毁平台句柄、令后续 `Play()` 成为安全 no-op。该钩子必须幂等。
- `Detach()` 还会把 source 从注册表摘除，所以即使 source 在后端之后析构也不会回访已销毁的注册表。
- `BackendRuntime::SetAudioBackend()` 会在销毁旧后端之前调用 `Quiesce()`。未实现 `Quiesce()` 的自定义后端不会得到这层保护。

`Source 不得在创建它的 Backend 销毁后继续存在` 依旧是使用者的责任，但引擎现在会主动解除这种引用，而不是让悬垂指针在析构时崩溃。

## 注册自定义格式解码器

解码器按扩展名直接注册到全局 `AudioDecoderRegistry`：

```cpp
#include <Backend/Runtime/BackendRuntime.hpp>

auto& decoders = atom::backend::BackendRuntime::GetInstance().AudioDecoders();
decoders.Register(".ogg", [] { return std::make_unique<MyOggDecoder>(); }, "MyOgg");
```

### 解码链的职责边界

一个扩展名可以挂一条**候选链**（首选 + 兜底）。谁负责什么，必须保持固定：

| 层 | 职责 | 明确不做的事 |
|---|---|---|
| 解码器 `IAudioDecoder` | 判断这个文件自己能不能解；不能解时用 `DecoderOpenStatus` 说明**原因**（`UnsupportedFormat` / `InvalidData` / `IoError`） | 不挑别的解码器、不自己回退、不判断引擎格式上限 |
| 注册表 `AudioDecoderRegistry` | 按扩展名维护有序候选链（`Register` 首选、`RegisterFallback` 兜底、`Replace` 替换整条链、候选可带诊断用名字） | 不打开文件、不做格式校验、不写日志 |
| 加载器 `AudioClipLoader` | 按顺序尝试候选、校验引擎格式契约、汇总诊断（全部失败才 `LOG_WARNING`） | 不决定顺序、不认识具体解码器 |
| 调用方 / 引擎默认策略 | 决定顺序（`BackendRuntime::RegisterDefaultAudioDecoders`，项目可覆盖） | 不重复实现尝试逻辑 |

回退的触发条件因此是**显式**的：候选返回非 `Opened` 就继续下一个，并由 loader 汇总成一条警告，例如

```text
OpenStreaming: every registered decoder declined the file [WavProf (i/o error), SDL3Wav (invalid data)]: x.wav
```

注意区分"能力边界"和"数据损坏"：前者（WavProf 遇到 ADPCM/µ-Law）是链式回退的正常路径，用 `LOG_DEBUG`；后者只在所有候选都无法处理时才升级为警告。截断文件是否容忍由各解码器自己决定——当前两个 WAV 解码器都接受截断（读到 EOF 即停），这属于解码器策略，不是链的职责。

其他约束：

- `DecoderInfo::bits_per_sample` 只允许 8 / 16 / 32（或 32 + `is_float`）。读取 packed 24-bit PCM 的解码器必须**自己**在解码时展宽为 32-bit 并上报 32（`WavProfDecoder` 与 `SDL3WavDecoder` 都是如此），`AudioClipLoader` 会拒绝上报 24 的解码器。
- 显式注入的注册表（见下）可以用 `BackendRuntime::RegisterDefaultAudioDecoders(registry)` 一次性补齐引擎默认解码器。

### 内置 .wav 的两个实现

```cpp
auto& decoders = atom::backend::BackendRuntime::GetInstance().AudioDecoders();

decoders.Register(".wav", atom::backend::audio_decoder::CreateWavProfDecoder, "WavProf");          // 首选
decoders.RegisterFallback(".wav", atom::backend::audio_decoder::CreateSDL3WavDecoder, "SDL3Wav"); // 兜底
// 只想要其中一个：
// decoders.Replace(".wav", atom::backend::audio_decoder::CreateSDL3WavDecoder, "SDL3Wav");
```

| | WavProf（自研，首选） | SDL3Wav（SDL_LoadWAV，兜底） |
|---|---|---|
| 解码方式 | 流式，64 KiB 窗口边读边推 | 一次性把整段 PCM 解进内存 |
| 常驻内存 | 约 64 KiB | 等于整段 PCM |
| 34 MB / 16-bit PCM WAV 全量读完（open + 解码） | 7.2 ms | 12.9 ms（其中 10.7 ms 在 open 里） |
| Seek | 字节偏移，逐样本精确 | 内存游标，逐样本精确 |
| 格式覆盖 | PCM U8/S16/S24/S32、IEEE float | 以上全部，外加 MS ADPCM、IMA ADPCM、A-Law、µ-Law |
| 依赖 | 无（自带 RIFF 解析） | SDL3 |

选型结论：纯 PCM 用 WavProf（更快、内存恒定），WavProf 声明 `UnsupportedFormat` 的压缩 WAV 自动落到 SDL3Wav。等自研解码器补齐压缩格式后，把两行顺序对调或删掉兜底即可。

### 关于 24-bit PCM：为什么没有 `Signed24`

高解析度素材常见的 packed 24-bit（3 字节/样本）**没有**进 `AudioSampleFormat`，原因是 **SDL3 的 `SDL_AudioFormat` 也不存在 24-bit 格式**（只有 U8/S8/S16/S32/F32/F64）。播放设备与 `SDL_AudioStream` 都无法接收 packed 24-bit，所以展宽动作无法避免，只能选择放在哪一层：

- 现在的做法（保持）：解码器在解码时把 24-bit 展宽成 S32，`DecoderInfo` 上报 32。代价是内存与带宽多 1/3，收益是零拷贝地进入播放链路，热路径没有任何逐样本转换；`WavProfDecoder` 使用调用方缓冲区的尾部做原地展宽，因此没有额外的一次整块拷贝。
- 若将来真的出现 24-bit 内存瓶颈，可行方案是在 `AudioSampleFormat` 增加 `Signed24` 作为**解码侧载体**，并在流式音源的推送路径里转换到设备支持的格式。代价是每个 PCM 块都要在热路径上做一次转换，且所有后端都要处理该格式，收益仅为缓冲区少 1/4 —— 目前不划算。

两个内置 WAV 解码器的 24-bit 输出已交叉验证为**逐字节一致**（705600 字节、0 差异）。

### 解码器与 Seek

播放层的 Seek 能力完全由解码器决定，契约是：

```cpp
[[nodiscard]] auto IsSeekable() const -> bool;              // 声明能力
auto SeekToFrame(std::uint64_t frame) -> bool;              // 定位到绝对 PCM 帧
```

- 默认实现只支持 `frame == 0`（等价于 `Rewind()`），因此**新解码器即使不实现 Seek 也能工作**，只是 `MusicPlayer::Seek` 对非 0 目标返回 `false`。
- 内置解码器：`WavProfDecoder`（未压缩 PCM，按字节偏移，逐样本精确）、`Minimp3Decoder`（`mp3dec_ex_seek`，采样级精确）。
- 传入的 frame 可以超出总长度：实现应当钳制到最后一帧并返回 `true`，让"定位到末尾"这种行为可用。
- 播放后端分两条路径：整段 PCM 在内存中的 buffered 音源直接移动播放游标（SDL_mixer 用 `MIX_SetTrackPlaybackPosition`）；streaming 音源则停解码线程、清空环形缓冲与 SDL 队列、调用 `SeekToFrame()` 后重启。注意 SDL_mixer **不允许**对自己创建为 `MIX_SetTrackAudioStream` 的 track 做 seek，所以 streaming 音乐的位置必须由解码器提供。

## 访问当前后端

```cpp
auto& runtime = atom::backend::BackendRuntime::GetInstance();

runtime.Audio();                 // IAudioBackend&，永不抛异常
runtime.TryAudio();              // IAudioBackend*，无活动后端时为 nullptr
runtime.AcquireAudioBackend();   // std::shared_ptr<IAudioBackend>，创建 source 时用这个
```

- `Audio()` 在没有活动后端时返回 `NullAudioBackend`（所有工厂返回 `nullptr`），不会抛 `std::runtime_error`；旧实现在切换窗口抛异常，会把后台加载线程直接变成 `std::terminate`。
- 创建 source 请使用 `AcquireAudioBackend()`：它持有一份强引用，保证整个创建过程里后端对象（及其子系统租约）不会被并发的后端切换销毁。

## 全局切换语义

```cpp
runtime.SetAudioBackend(atom::backend::AudioBackendId::Sdl3Mixer);
```

切换分四步：

1. **先创建**新后端实例。创建失败直接返回 `false`，当前后端与它拥有的全部 source 完全不受影响（真正的回滚，旧 ID 也不会被清空）；
2. 通知 `OnAudioBackendChanging()`：所有接入全局 Runtime 的 Player 停止当前声音、删除全部注册 ID，并销毁 Music Source、SFX VoicePool 和缓存；缓存了播放信息的页面也应在此失效自己的缓存；
3. 调用旧后端的 `Quiesce()`，把它创建的其余 source 一并失效；
4. 原子替换后端并递增 generation，然后通知 `OnAudioBackendChanged()`。

系统不迁移播放位置，也不自动重播；切换后由页面或后续场景重新调用 `Load/Play`。

### Generation（缓存失效判定）

```cpp
const auto generation = runtime.GetAudioBackendGeneration();
// ... 稍后 ...
if (runtime.GetAudioBackendGeneration() != generation) {
    // 这份 id / source / 元数据缓存是上一个后端产生的，已经作废。
}
```

每次成功切换 generation +1。跨切换的异步加载（例如后台线程正在 `MusicPlayer::Load`）应该"进入时快照、提交时比对"，否则会把旧后端创建的 source 当成有效结果写进缓存——这正是示例 `MusicCard` 早期出现"预解码过的曲子无法播放、没预解码的反而能播"的原因。

## 切换时机约束

Atom 不推荐在大量 ID 已注册时切换 Backend，例如正式 Gameplay、战斗或关卡运行期间。建议只在主菜单或"设置"页面切换，此时通常只有零到两个背景音乐/UI 音效 ID，清理和重新加载成本明确。

`SFXPlayer::Reset()` 会连同解码后的 PCM 缓存一起清空，因此切换后所有音效都需要重新解码。Backend 切换应从**主线程**发起：`Quiesce()` / `Detach()` 在主线程执行，Listener 回调也在主线程。跨线程创建 source 请使用 `AcquireAudioBackend()`。

Gameplay 状态检测目前尚未由 Runtime 强制执行，项目应自行限制设置入口。切换过程本身不再是"无后端窗口"：`Audio()`/`TryAudio()` 的调用者永远只会看到旧后端或新后端。

## 显式注入

测试和独立工具可以绕过全局 Runtime：

```cpp
atom::audio::AudioDecoderRegistry test_decoders;
atom::backend::BackendRuntime::RegisterDefaultAudioDecoders(test_decoders); // 补齐 .wav/.mp3
atom::MusicPlayer music{fake_backend, test_decoders, mixer};
atom::AudioClipCache clips{test_decoders};
atom::SFXPlayer sfx{fake_backend, clips, mixer};
```

显式注入的 Player 不注册全局切换监听器，也不会在全局 Backend 改变时自动清空。

## 当前内置

```text
Audio Playback Backend:
- sdl3（默认）          : 原生 SDL3 音频流；无 pitch/pan/3D 扩展（Doppler 等效果自动 fallback）
- sdl3_mixer            : SDL3_mixer；提供 IPitchControl / IPanControl / ISpatialEmitter，
                          buffered（SFX）与 streaming（Music）source 都实现该扩展集

Default Decoders（BackendRuntime::RegisterDefaultAudioDecoders）:
- .wav → 首选 WavProfDecoder（Backend/Audio/Decoder/WavProf；流式、任意帧 Seek）
         兜底 SDL3WavDecoder（Backend/Audio/Decoder/SDL3Wav；SDL_LoadWAV，覆盖 ADPCM/A-Law/µ-Law）
- .mp3 → Minimp3Decoder（minimp3 封装，Backend/Audio/Decoder/minimp3；采样级 Seek）
```

重复设置当前后端不触发清理或重建。新增播放后端后，只要它实现了 `Quiesce()`，同一套全局切换协议无需修改 Player。

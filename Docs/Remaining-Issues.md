# Atom Engine 迁移未解决问题清单

> 记录 SDL3 迁移完成后仍存在的已知问题与改进方向。
> 已修复的问题（EventType::None 崩溃、ImGui 双次销毁、ESC 退出、FPS 限制、Stop 死锁、悬空指针、SDL 初始化顺序、SFX 格式不匹配等）已从清单移除。

---

## 1. Music 没有真正的流式播放

**严重性：** 中（功能正确但内存浪费）

**描述：**
`Music::Load()` 通过 `SDL_LoadWAV` 将整个 WAV 文件解码到 `std::vector<uint8_t>` 中。5 分钟的 44100Hz 16-bit 立体声 WAV 占用 ~50MB 内存。对于播放时间长的背景音乐，这显然是浪费。

**改进方案：**
实现 128KB 环形缓冲区流式解码架构：
```
[磁盘文件] → [分块解码器] → [128K 环形缓冲] → [SDL_AudioStreamCallback] → [音频设备]
```
详见 `Docs/Audio-Backend-Plan.md` 第 1 节。

**涉及文件：**
- `Media/Audio/Backend/SDL3MusicSource.cpp` — 需要重构为 StreamingMusicSource
- 需要新的文件句柄解码器（配合 IAudioDecoder）

---

## 2. SFX 不支持重叠播放

**严重性：** 高（功能缺陷）

**描述：**
`SDL3SFXSource` 每个 SFX id 只有一个 `SDL_AudioStream`。`Play()` 先 `SDL_ClearAudioStream` 再 push，所以：
```cpp
sfx.Play("gunshot");  // 开始播放
sfx.Play("gunshot");  // 清空上一个，重新开始 → 前一个被切断
```
这在射击游戏、脚步声等场景中是完全不能接受的。

**改进方案：**
为每个 SFX id 维护一个声部池（voice pool），默认 8 个 voice。`Play()` 时找第一个 Stopped 的 voice 复用，不够则新建。详见 `Docs/Audio-Backend-Plan.md` 第 3 节。

**涉及文件：**
- `Media/Audio/SFX/SFX.hpp` — 需要新增 `SFXEntry` 多实例池
- `Media/Audio/SFX/SFX.cpp` — `Play()` 需要改为 voice 分配逻辑

---

## 3. IAudioDecoder 未接入 Music/SFX 加载路径

**严重性：** 中（接口已定义但未使用）

**描述：**
已定义以下组件但还没有被 `Music::Load()` 或 `SFXManager::LoadSFXFiles()` 使用：
- `Engine/Interfaces/IAudioDecoder.hpp` — 抽象解码器接口
- `Media/Audio/Decoder/AtomWavDecoderBackend.hpp/cpp` — Atom WavDecoder 后端
- `ThirdParty/Lib/AtomWavDecoder/WavDecoder.hpp/cpp` — C++ WAV 解码器

当前 `Music::Load()` 仍硬编码使用 `SDL_LoadWAV`，无法切换解码器。

**改进方案：**
1. 为 SDL3 的 `SDL_LoadWAV` 写一个 `SDL3WavDecoder`（实现 IAudioDecoder）
2. 添加 `DecoderRegistry` 自动按扩展名选择解码器
3. 将 `Music::Load()` 和 `SFXManager::LoadSFXFiles()` 切换到 `DecoderRegistry`

**涉及文件：**
- `Media/Audio/Music/Music.cpp` — `Load()` 需要适配
- `Media/Audio/SFX/Manager/SFXManager.cpp` — `LoadSFXFiles()` 需要适配
- `Engine/Interfaces/IAudioDecoder.hpp` — 已定义待使用
- `Media/Audio/Decoder/AtomWavDecoderBackend.hpp` — 已实现待使用

---

## 4. 引擎存在两个 WAV 解码器

**严重性：** 低（代码冗余）

**描述：**
当前项目中有两个功能完全重复的 WAV 解码器：

| 位置 | 命名空间 | 后端 | 来源 |
|------|---------|------|------|
| `Media/Decoder/WavRiff/WavDecoder.hpp` | `atom::media` | `std::ifstream` | 引擎自己的 C++ 实现 |
| `ThirdParty/Lib/AtomWavDecoder/WavDecoder.hpp` | `atom` | `FILE*` | 从 WavProf 复制的 C++ 改写 |

**建议：**
合并为一个。保留 `Media/Decoder/WavRiff/` 中的解码器（已经是 C++，使用 `ifstream`），删除 ThirdParty 中的副本，或反之。

---

## 5. 视频模块状态未知

**严重性：** 中（需要验证）

**描述：**
`Media/Video/Video.hpp` / `Media/Video/Video.cpp` 在 `engine_media` 中编译但尚未验证是否适配 SDL3。如果它使用 FFmpeg 解码 + SDL3 渲染，需要确认 API 调用是否正确。

**涉及文件：**
- `Media/Video/Video.hpp`
- `Media/Video/Video.cpp`

---

## 6. 每个 Source 创建独立 Device Stream

**严重性：** 低（性能潜在风险）

**描述：**
每个 `SDL3MusicSource` 和 `SDL3SFXSource` 都调用 `SDL_OpenAudioDeviceStream()` 创建独立的设备绑定。大量 SFX 同时播放时（> 20 个），每个 stream 有自己的格式转换器和缓冲区，可能产生额外开销。

**改进方案（按复杂度排序）：**
- **A）暂不处理** — SDL3 内部混音效率足够，< 32 个 stream 不是问题
- **B）Stream 池化** — 固定 pool（如 16 个），播放时借用，停止时归还
- **C）共享 SFX stream** — 软件混音后推入单个 SDL_AudioStream

详见 `Docs/Audio-Backend-Plan.md` 第 6 节。

---

## 7. 格式覆盖不全（仅 WAV）

**严重性：** 低（功能可扩展）

**描述：**
当前仅支持 WAV 格式（通过 `SDL_LoadWAV`）。MP3、FLAC、OGG 等常见格式不支持。

**计划（后续版本）：**

| 格式 | 解码库 | 许可证 |
|------|--------|--------|
| MP3  | minimp3 | CC0 |
| FLAC | dr_flac | 公有领域 |
| OGG  | stb_vorbis | MIT |

---

## 8. 单元测试缺失

**严重性：** 中（回归风险）

**描述：**
迁移后的 SDL3 后端（渲染、音频、窗口）没有任何单元测试。无法快速验证修改是否破坏现有功能。

---

## 9. CMakeLists.txt 节编号混乱

**严重性：** 低（代码整洁）

**描述：**
多次插入/删除库后，CMakeLists.txt 中的注释编号已不连续（#4 → #6 → #8 等），后续维护容易引起混淆。建议统一重新编号或移除编号改用描述性标题。

---

## 10. SDL_AudioStreamCallback 未使用

**严重性：** 低（性能优化）

**描述：**
`SDL3MusicSource` 用独立线程 + 10ms sleep 轮询推数据到 stream。SDL3 原生支持拉取式回调模型（`SDL_AudioStreamCallback`），由 SDL 在需要数据时自动回调，延迟更低、CPU 占用更省。可与第 1 条（128K 流式）一同重构。

---

## 优先级建议

| 优先级 | 问题 | 理由 |
|--------|------|------|
| 🔴 高 | 2. SFX 重叠播放 | 功能缺陷，影响游戏逻辑 |
| 🔴 高 | 3. IAudioDecoder 未接入 | 接口定义完善但无用，越早接上越好 |
| 🟡 中 | 1. Music 流式 | 影响内存占用，长音乐场景明显 |
| 🟡 中 | 5. 视频模块验证 | 未知风险 |
| 🟡 中 | 8. 单元测试 | 回归风险 |
| 🟢 低 | 4. 双解码器合并 | 代码整洁 |
| 🟢 低 | 6. Device Stream | 在当前规模下可忽略 |
| 🟢 低 | 7. 格式覆盖 | 后续版本 |
| 🟢 低 | 9. CMake 编号 | 代码整洁 |
| 🟢 低 | 10. Callback 优化 | 后续性能优化 |

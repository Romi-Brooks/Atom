# Atom Engine 迁移未解决问题清单

> 记录 SDL3 迁移完成后仍存在的已知问题与改进方向。
>
> ✅ 已解决（2026/7/12）：48000 Hz 白噪声/咔哒声修复、SFX 硬编码格式修复、SFX 重叠播放（Voice Pool）、IAudioDecoder 接入 Music/SFX 加载路径、SDL_AudioStreamCallback 回调模式、视频模块验证、CMakeLists 编号重排。
>
> 已修复的问题（EventType::None 崩溃、ImGui 双次销毁、ESC 退出、FPS 限制、Stop 死锁、悬空指针、SDL 初始化顺序、SFX 格式不匹配、采样率重采样等）已从清单移除。

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
- `SDL_AudioStreamCallback` 已启用（#10），可直接配合流式播放使用

---

## 2. 每个 Source 创建独立 Device Stream（原 #6）

**严重性：** 低（性能潜在风险）

**描述：**
每个 `SDL3MusicSource` 和 `SDL3SFXSource` 都调用 `SDL_OpenAudioDeviceStream()` 创建独立的设备绑定。大量 SFX 同时播放时（> 20 个），每个 stream 有自己的格式转换器和缓冲区，可能产生额外开销。

**改进方案（按复杂度排序）：**
- **A）暂不处理** — SDL3 内部混音效率足够，< 32 个 stream 不是问题
- **B）Stream 池化** — 固定 pool（如 16 个），播放时借用，停止时归还
- **C）共享 SFX stream** — 软件混音后推入单个 SDL_AudioStream

详见 `Docs/Audio-Backend-Plan.md` 第 6 节。

---

## 3. 格式覆盖不全（仅 WAV）（原 #7）

**严重性：** 低（功能可扩展）

**描述：**
当前仅支持 WAV 格式（通过解码器注册表）。MP3、FLAC、OGG 等常见格式不支持。

**计划（后续版本）：**

| 格式 | 解码库 | 许可证 |
|------|--------|--------|
| MP3  | minimp3 | CC0 |
| FLAC | dr_flac | 公有领域 |
| OGG  | stb_vorbis | MIT |

**前置条件：** `IAudioDecoder` + `DecoderRegistry` 已就绪（#3），新增格式只需实现 `IAudioDecoder` 后注册到 `DecoderRegistry`。

---

## 4. 单元测试缺失（原 #8）

**严重性：** 中（回归风险）

**描述：**
迁移后的 SDL3 后端（渲染、音频、窗口）没有任何单元测试。无法快速验证修改是否破坏现有功能。

---

## 优先级建议

| 优先级 | 问题 | 理由 |
|--------|------|------|
| 🟡 中 | 1. Music 流式 | 影响内存占用，长音乐场景明显。回调管线已就绪，可开始实现 |
| 🟡 中 | 4. 单元测试 | 回归风险 |
| 🟢 低 | 2. Device Stream | 在当前规模下可忽略 |
| 🟢 低 | 3. 格式覆盖 | 后续版本，解码器框架已就绪 |

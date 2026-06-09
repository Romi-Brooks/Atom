# Atom Engine — SFML 到 SDL3 迁移方案

## 1. 背景与目标

### 1.1 为什么要迁移

| 当前状态（SFML 3.0） | 目标状态（SDL3） |
|---------------------|----------------|
| 引擎 20 个文件深度耦合 SFML | 引擎仅接口层依赖实现，底层零 SFML |
| 公有接口暴露 `sf::` 类型 | 引擎模块使用抽象接口，不感知底层库 |
| 跨平台能力有限 | Steam Deck / 主机级支持 |
| SFML 维护缓慢，社区较小 | SDL 商业支持（Valve），社区活跃 |
| 无现代 GPU API | SDL3 GPU API（Vulkan/Metal/D3D12） |
| 无原生 PNG 加载 | SDL3.4+ 内置 `SDL_LoadPNG` |
| 音频无混音/流式控制 | SDL3 AudioStream 提供内置混音、重采样、增益 |

### 1.2 架构策略

```
迁移前 (SFML 耦合):
┌──────────────────────────────┐
│       引擎模块 (Screen,       │
│     Entity, Music, SFX...)    │
│         ↕ 直接依赖            │
│      sf::RenderWindow         │
│      sf::Texture / sf::Music  │
│      sf::SoundBuffer ...      │
└──────────────────────────────┘

迁移后 (接口解耦):
┌──────────────────────────────┐
│       引擎模块 (Screen,       │
│     Entity, Music, SFX...)    │
│         ↕ 依赖接口            │
│  IRenderTarget / ITexture     │
│  IAudioSource / IAudioBuffer  │
│         ↕ 实现                │
│  SDL3RenderWindow             │
│  SDL3Texture / SDL3AudioSrc   │
│  ...                          │
└──────────────────────────────┘
```

**核心理念**：
- **保留抽象接口层**为永久架构层，引擎模块通过接口调用底层功能
- **SDL3 是唯一实现**，迁移完成后 SFML 实现代码全部移除
- **不保留多后端切换能力**，接口层的存在是为了模块边界清晰、可测试性，而非可插拔
- **接口不泄露底层类型**，SDK 用户只需包含接口头文件，不依赖 SDL3

---

## 2. 接口设计

### 2.1 接口分层

```
引擎上层模块 (依赖接口)
  ScreenManager / Music / SFX / Entity / Debugger
        ↕
抽象接口层 (纯虚类)
  IRenderTarget  ITexture  IRenderWindow
  IAudioSource   IAudioBuffer  ISFXManager
  IMediaDecoder
        ↕
SDL3 实现 (唯一后端)
  SDL3RenderWindow  SDL3Texture  SDL3AudioSource
  SDL3AudioBuffer   SDL3MediaDecoder
```

### 2.2 IRender 接口族

#### IRenderTarget

渲染目标抽象，替代 `sf::RenderWindow&` 作为 Draw 的目标。

```cpp
// File: Engine/Interfaces/IRenderTarget.hpp
namespace atom {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    static constexpr Color Black()   { return {0, 0, 0}; }
    static constexpr Color White()   { return {255, 255, 255}; }
    static constexpr Color Red()     { return {255, 0, 0}; }
    static constexpr Color Green()   { return {0, 255, 0}; }
    static constexpr Color Blue()    { return {0, 0, 255}; }
};

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    virtual auto Clear(const Color& color = Color::Black()) -> void = 0;
    virtual auto Display() -> void = 0;
    [[nodiscard]] virtual auto GetSize() const -> Vec2 = 0;
    virtual auto SetViewport(const Rect& viewport) -> void = 0;
    [[nodiscard]] virtual auto GetViewport() const -> Rect = 0;
};

} // namespace atom
```

#### ITexture

纹理抽象，替代 `sf::Texture`。

```cpp
// File: Engine/Interfaces/ITexture.hpp
namespace atom {

class ITexture {
public:
    virtual ~ITexture() = default;
    virtual auto LoadFromFile(const std::string& path) -> bool = 0;
    virtual auto Update(const uint8_t* pixels, uint32_t width, uint32_t height) -> bool = 0;
    [[nodiscard]] virtual auto GetSize() const -> Vec2 = 0;
};

} // namespace atom
```

#### IRenderWindow

窗口抽象，替代 `sf::RenderWindow`。

```cpp
// File: Engine/Interfaces/IRenderWindow.hpp
namespace atom {

enum class EventType {
    Closed, KeyPressed, KeyReleased,
    MouseMoved, MouseButtonPressed, MouseButtonReleased, Resized,
};

struct KeyEvent     { int32_t scancode, keycode; bool alt, ctrl, shift; };
struct MouseEvent   { float x, y; int32_t button; };
struct ResizeEvent  { uint32_t width, height; };

struct IEvent {
    EventType type;
    std::variant<KeyEvent, MouseEvent, ResizeEvent> data;
};

class IRenderWindow : public IRenderTarget {
public:
    ~IRenderWindow() override = default;
    virtual auto Initialize(const std::string& title, Vec2 resolution) -> void = 0;
    [[nodiscard]] virtual auto IsOpen() const -> bool = 0;
    virtual auto Shutdown() -> void = 0;
    [[nodiscard]] virtual auto PollEvent() -> std::optional<IEvent> = 0;
    virtual auto SetFPS(uint32_t fps) -> void = 0;
    [[nodiscard]] virtual auto GetFPS() const -> uint32_t = 0;
    [[nodiscard]] virtual auto GetNativeHandle() const -> void* = 0;
};

} // namespace atom
```

#### Screen（已有基类，替换 SFML 类型）

```cpp
// File: Window/Screen.hpp（迁移后）
namespace atom {

class Screen {
public:
    virtual ~Screen() = default;
    virtual auto Render(IRenderTarget& target) -> void = 0;
    virtual auto HandleEvent(const IEvent& event) -> bool = 0;
    virtual auto Update(float deltaTime) -> void = 0;
    virtual auto OnActivate() -> void {}
    virtual auto OnDeactivate() -> void {}
};

} // namespace atom
```

### 2.3 IAudio 接口族

#### IAudioBuffer

音频数据缓冲抽象，替代 `sf::SoundBuffer`。

```cpp
// File: Engine/Interfaces/IAudioBuffer.hpp
namespace atom {

class IAudioBuffer {
public:
    virtual ~IAudioBuffer() = default;
    virtual auto LoadFromFile(const std::string& path) -> bool = 0;
    [[nodiscard]] virtual auto GetSampleRate() const -> uint32_t = 0;
    [[nodiscard]] virtual auto GetChannelCount() const -> uint8_t = 0;
    [[nodiscard]] virtual auto GetDuration() const -> float = 0;
    [[nodiscard]] virtual auto GetSamples() const -> const int16_t* = 0;
    [[nodiscard]] virtual auto GetSampleCount() const -> uint64_t = 0;
};

} // namespace atom
```

#### IAudioSource

音频播放实例抽象，替代 `sf::Sound` 和 `sf::Music` 的播放功能。

```cpp
// File: Engine/Interfaces/IAudioSource.hpp
namespace atom {

enum class AudioSourceState { Stopped, Playing, Paused };

class IAudioSource {
public:
    virtual ~IAudioSource() = default;
    virtual auto Play() -> void = 0;
    virtual auto Stop() -> void = 0;
    virtual auto Pause() -> void = 0;
    [[nodiscard]] virtual auto GetState() const -> AudioSourceState = 0;
    virtual auto SetVolume(float volume) -> void = 0;
    [[nodiscard]] virtual auto GetVolume() const -> float = 0;
    virtual auto SetLooping(bool loop) -> void = 0;
    [[nodiscard]] virtual auto IsLooping() const -> bool = 0;
    virtual auto SetPlayingOffset(float seconds) -> void = 0;
    [[nodiscard]] virtual auto GetPlayingOffset() const -> float = 0;
};

} // namespace atom
```

#### ISFXManager

音效资源管理器抽象，替代 `SFXManager` 对 `sf::SoundBuffer` 的管理。

```cpp
// File: Engine/Interfaces/ISFXManager.hpp
namespace atom {

class ISFXManager {
public:
    virtual ~ISFXManager() = default;
    virtual auto Load(const std::string& id, const std::string& filePath) -> bool = 0;
    [[nodiscard]] virtual auto GetBuffer(const std::string& id) -> IAudioBuffer* = 0;
    [[nodiscard]] virtual auto Has(const std::string& id) const -> bool = 0;
    virtual auto Unload(const std::string& id) -> bool = 0;
    virtual auto UnloadAll() -> void = 0;
    [[nodiscard]] virtual auto GetLoadedCount() const -> size_t = 0;
};

} // namespace atom
```

### 2.4 IMedia 接口族

#### IMediaDecoder

媒体解码器抽象，用于 FFmpeg 视频流解码。

```cpp
// File: Engine/Interfaces/IMediaDecoder.hpp
namespace atom {

struct MediaFrameInfo {
    uint32_t width, height;
    float frameRate, duration;
};

class IMediaDecoder {
public:
    virtual ~IMediaDecoder() = default;
    virtual auto Open(const std::string& path) -> bool = 0;
    virtual auto Close() -> void = 0;
    [[nodiscard]] virtual auto GetInfo() const -> MediaFrameInfo = 0;
    virtual auto DecodeNextFrame() -> bool = 0;
    [[nodiscard]] virtual auto GetFrameData() const -> const uint8_t* = 0;
    virtual auto Seek(float seconds) -> bool = 0;
};

} // namespace atom
```

---

## 3. SDL3 实现方案

### 3.1 窗口模块 — SDL3RenderWindow

```cpp
// ───── SDL3 实现：SDL3RenderWindow ─────
#include <SDL3/SDL.h>
#include <Engine/Interfaces/IRenderWindow.hpp>

namespace atom {

class SDL3RenderWindow : public IRenderWindow {
private:
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    uint32_t      fps_limit_ = 60;
    bool          open_ = false;

public:
    SDL3RenderWindow() = default;
    ~SDL3RenderWindow() override { Shutdown(); }

    auto Initialize(const std::string& title, Vec2 resolution) -> void override {
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);
        window_ = SDL_CreateWindow(
            title.c_str(),
            static_cast<int>(resolution.x),
            static_cast<int>(resolution.y),
            SDL_WINDOW_RESIZABLE
        );
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        open_ = true;
    }

    auto Clear(const Color& color) -> void override {
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        SDL_RenderClear(renderer_);
    }

    auto Display() -> void override {
        SDL_RenderPresent(renderer_);
    }

    [[nodiscard]] auto PollEvent() -> std::optional<IEvent> override {
        SDL_Event ev;
        if (!SDL_PollEvent(&ev)) return std::nullopt;

        IEvent result;
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                result = {EventType::Closed, {}};
                break;
            case SDL_EVENT_KEY_DOWN:
                result = {EventType::KeyPressed,
                    KeyEvent{ev.key.scancode, ev.key.key,
                             static_cast<bool>(ev.key.mod & SDL_KMOD_ALT),
                             static_cast<bool>(ev.key.mod & SDL_KMOD_CTRL),
                             static_cast<bool>(ev.key.mod & SDL_KMOD_SHIFT)}};
                break;
            // ... 处理其他事件类型
        }
        return result;
    }

    [[nodiscard]] auto GetNativeHandle() const -> void* override {
        return static_cast<void*>(window_);
    }

    auto Shutdown() -> void override {
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
        open_ = false;
    }

    auto IsOpen() const -> bool override { return open_; }
    auto SetFPS(uint32_t fps) -> void override { fps_limit_ = fps; }
    [[nodiscard]] auto GetFPS() const -> uint32_t override { return fps_limit_; }
    [[nodiscard]] auto GetSize() const -> Vec2 override {
        int w, h;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        return {static_cast<float>(w), static_cast<float>(h)};
    }
    // ... SetViewport / GetViewport / ...
};

} // namespace atom
```

**SFML → SDL3 方法映射**：

| 接口方法 | SFML 实现 | SDL3 实现 |
|---------|-----------|----------|
| `Clear(color)` | `window.clear(sf::Color(...))` | `SDL_SetRenderDrawColor + SDL_RenderClear` |
| `Display()` | `window.display()` | `SDL_RenderPresent` |
| `PollEvent()` | `window.pollEvent()` | `SDL_PollEvent` |
| `GetSize()` | `window.getSize()` | `SDL_GetWindowSizeInPixels` |
| 帧计时 | `sf::Clock / sf::Time` | `SDL_GetTicksNS` |

### 3.2 纹理模块 — SDL3Texture

```cpp
// ───── SDL3 实现：SDL3Texture ─────
#include <SDL3/SDL.h>
#include <Engine/Interfaces/ITexture.hpp>

namespace atom {

class SDL3Texture : public ITexture {
private:
    SDL_Texture* texture_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Vec2 size_;

public:
    explicit SDL3Texture(SDL_Renderer* renderer) : renderer_(renderer) {}
    ~SDL3Texture() override { if (texture_) SDL_DestroyTexture(texture_); }

    auto LoadFromFile(const std::string& path) -> bool override {
        SDL_Surface* surface = SDL_LoadPNG(path.c_str());  // SDL3.4+ 内置
        if (!surface) return false;
        if (texture_) SDL_DestroyTexture(texture_);
        texture_ = SDL_CreateTextureFromSurface(renderer_, surface);
        size_ = {static_cast<float>(surface->w), static_cast<float>(surface->h)};
        SDL_DestroySurface(surface);
        return texture_ != nullptr;
    }

    auto Update(const uint8_t* pixels, uint32_t width, uint32_t height) -> bool override {
        int pitch = static_cast<int>(width * 4);  // RGBA8888
        return SDL_UpdateTexture(texture_, nullptr, pixels, pitch);
    }

    [[nodiscard]] auto GetSize() const -> Vec2 override { return size_; }
};

} // namespace atom
```

### 3.3 音频模块 — SDL3 实现

#### 架构变化

```
SFML 架构:                    SDL3 架构:
┌─────────┐   加载           ┌───────────────┐
│ sf::Music│ ─────→ [OGG解码] │ 外部解码器     │
│ (流式)   │  内部解码        │ (dr_mp3 /     │
└─────────┘                  │  libvorbis)    │
                             └───────┬───────┘
┌─────────┐   加载                  │ PCM 数据
│sf::Sound│ ─────→ [PCM缓冲]        ▼
│(一次性)  │    sf::SoundBuffer  ┌───────────────┐
└─────────┘                     │SDL_AudioStream │
                                │ (内置混音/增益) │
                                └───────┬───────┘
                                        │
                                        ▼
                                ┌───────────────┐
                                │逻辑音频设备    │
                                └───────────────┘
```

**关键变化**：
- `sf::Music` 内置 OGG/MP3 解码 → 使用 `dr_mp3` / `libvorbis` 等外部解码器
- `sf::Sound` + `sf::SoundBuffer` → `SDL_AudioStream` + PCM 缓冲池
- SDL3 的 logical audio device 允许不同模块拥有独立音频设备
- `VolumeManager` 无变化（纯数值管理器，无 SFML 依赖）

#### SDL3MusicSource

```cpp
// ───── SDL3 实现：SDL3MusicSource ─────
namespace atom {

class SDL3MusicSource : public IAudioSource {
private:
    SDL_AudioStream* stream_ = nullptr;
    std::thread decode_thread_;
    std::atomic<bool> playing_{false};
    std::atomic<bool> loop_{false};
    float volume_ = 100.0f;

public:
    explicit SDL3MusicSource(const std::string& filePath) {
        SDL_AudioDeviceID device = SDL_GetDefaultAudioDeviceID();
        stream_ = SDL_CreateAudioStream(&src_spec, &device_spec, nullptr);
        SDL_BindAudioStream(device, stream_);
        // 打开文件，准备解码器
    }

    ~SDL3MusicSource() override {
        Stop();
        if (stream_) SDL_DestroyAudioStream(stream_);
    }

    auto Play() -> void override {
        playing_ = true;
        SDL_SetAudioStreamGain(stream_, volume_ / 100.0f);

        decode_thread_ = std::thread([this]() {
            while (playing_) {
                // [解码循环]
                // 外部解码器解码 → PCM → SDL_PutAudioStreamData(stream_, pcm, len)
            }
        });

        SDL_ResumeAudioStreamDevice(stream_);
    }

    auto Stop() -> void override {
        playing_ = false;
        if (decode_thread_.joinable()) decode_thread_.join();
        SDL_ClearAudioStream(stream_);
        SDL_PauseAudioStreamDevice(stream_);
    }

    auto SetVolume(float volume) -> void override {
        volume_ = volume;
        SDL_SetAudioStreamGain(stream_, volume / 100.0f);
    }

    // ... GetState, Pause, SetLooping, etc.
};

} // namespace atom
```

#### SDL3AudioBuffer + SDL3SFXManager

```cpp
// ───── SDL3 实现：SDL3AudioBuffer ─────
namespace atom {

class SDL3AudioBuffer : public IAudioBuffer {
private:
    uint8_t*  data_ = nullptr;
    uint32_t  length_ = 0;
    SDL_AudioSpec spec_{};

public:
    SDL3AudioBuffer() = default;
    ~SDL3AudioBuffer() override { SDL_free(data_); }

    auto LoadFromFile(const std::string& path) -> bool override {
        // SDL_LoadWAV 或外部解码器
        return SDL_LoadWAV(path.c_str(), &spec_, &data_, &length_) != nullptr;
    }

    [[nodiscard]] auto GetSamples() const -> const int16_t* override {
        return reinterpret_cast<const int16_t*>(data_);
    }
    [[nodiscard]] auto GetSampleCount() const -> uint64_t override {
        return length_ / sizeof(int16_t);
    }
    [[nodiscard]] auto GetSampleRate() const -> uint32_t override { return spec_.freq; }
    [[nodiscard]] auto GetChannelCount() const -> uint8_t override { return spec_.channels; }
    [[nodiscard]] auto GetDuration() const -> float override {
        return static_cast<float>(length_) / (spec_.freq * spec_.channels * sizeof(int16_t));
    }
};

} // namespace atom
```

#### SDL3AudioSource（SFX 播放用）

```cpp
// ───── SDL3 实现：SDL3SFXSource ─────
namespace atom {

class SDL3SFXSource : public IAudioSource {
private:
    SDL_AudioStream* stream_ = nullptr;
    SDL_AudioDeviceID device_;
    const uint8_t* pcm_data_ = nullptr;
    uint32_t pcm_length_ = 0;
    float volume_ = 100.0f;

public:
    SDL3SFXSource() {
        device_ = SDL_GetDefaultAudioDeviceID();
        stream_ = SDL_CreateAudioStream(&src_spec, &device_spec, nullptr);
        SDL_BindAudioStream(device_, stream_);
    }

    ~SDL3SFXSource() override {
        if (stream_) SDL_DestroyAudioStream(stream_);
    }

    auto SetBuffer(const uint8_t* data, uint32_t length) -> void {
        pcm_data_ = data;
        pcm_length_ = length;
    }

    auto Play() -> void override {
        if (!pcm_data_) return;
        SDL_ClearAudioStream(stream_);
        SDL_SetAudioStreamGain(stream_, volume_ / 100.0f);
        SDL_PutAudioStreamData(stream_, pcm_data_, pcm_length_);
        SDL_ResumeAudioStreamDevice(stream_);
    }

    auto Stop() -> void override {
        SDL_ClearAudioStream(stream_);
        SDL_PauseAudioStreamDevice(stream_);
    }

    auto SetVolume(float volume) -> void override {
        volume_ = volume;
        SDL_SetAudioStreamGain(stream_, volume / 100.0f);
    }

    // ... GetState, Pause, etc.
};

} // namespace atom
```

### 3.4 媒体解码 — SDL3MediaDecoder

```cpp
// ───── SDL3 实现：SDL3MediaDecoder ─────
namespace atom {

class SDL3MediaDecoder : public IMediaDecoder {
private:
    // FFmpeg 上下文保持不变
    AVFormatContext* format_ctx_ = nullptr;
    AVCodecContext*  codec_ctx_ = nullptr;
    AVFrame*         frame_ = nullptr;
    AVFrame*         rgb_frame_ = nullptr;
    SwsContext*      sws_ctx_ = nullptr;
    MediaFrameInfo   info_{};

public:
    auto DecodeNextFrame() -> bool override {
        // FFmpeg 解码逻辑完全不变
        // av_read_frame → avcodec_send_packet → avcodec_receive_frame → sws_scale
        // ...
        return true;
    }

    [[nodiscard]] auto GetFrameData() const -> const uint8_t* override {
        return rgb_frame_->data[0];  // RGBA8888 数据
    }

    // ... Open, Close, Seek, GetInfo
};

} // namespace atom
```

FFmpeg 解码后的渲染输出从 `sf::Texture::update(pixels)` 改为 `ITexture::Update(pixels)`：

```cpp
// 迁移前
sf::Texture texture;
texture.update(rgbFrame->data[0]);  // ← SFML 耦合

// 迁移后
auto texture = std::make_unique<SDL3Texture>(renderer);
texture->Update(rgbFrame->data[0], width, height);  // ← 接口调用
```

### 3.5 ImGui 集成 — Debugger

```cpp
// ───── 迁移后的 Debugger ─────
namespace atom {

class Debugger {
private:
    bool attached_ = false;
    IRenderWindow* target_window_ = nullptr;

public:
    auto Attach(IRenderWindow& window) -> void {
        target_window_ = &window;

        // 使用 Dear ImGui 官方 SDL3 后端
        ImGui::CreateContext();
        ImGui_ImplSDL3_InitForSDLRenderer(
            static_cast<SDL_Window*>(window.GetNativeHandle())
        );
        ImGui_ImplSDLRenderer3_Init(
            static_cast<SDL_Renderer*>(/* 需要 IRenderWindow 暴露 SDL_Renderer* */)
        );

        // 挂载回调钩子
        // window.on_process_event_ / on_render_overlay_ ...
        attached_ = true;
    }

    auto Detach() -> void {
        if (!attached_) return;
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        attached_ = false;
    }

protected:
    virtual auto OnDrawOverlay() -> void {}
};

} // namespace atom
```

> **注意**：`IRenderWindow` 需要额外暴露 `GetSDLRenderer()` 方法供 ImGui 后端使用。这是唯一一处需要从接口获取 SDL3 具体类型的地方，ImGui 后端本身就是平台相关的。

---

## 4. 引擎模块接口化映射

以下表格说明每个现有引擎模块将如何依赖接口而非 SFML 具体类型。

### 4.1 Window 模块

| 当前文件 | 当前依赖 SFML | 接口化后依赖 | SDL3 实现类 |
|----------|---------------|-------------|------------|
| `RenderWindow.hpp` | `sf::RenderWindow`, `sf::Event`, `sf::Time` | `IRenderWindow` | `SDL3RenderWindow` |
| `Screen.hpp` | `sf::RenderWindow&`, `const sf::Event&` | `IRenderTarget&`, `const IEvent&` | （纯抽象不变） |
| `ScreenManager.hpp` | `sf::RenderWindow&`（传递） | `IRenderTarget&` | 仅替换接口类型 |
| `Debugger.hpp` | `<SFML/Graphics.hpp>`, `imgui-SFML` | `IRenderWindow&`（获取原生句柄） | `imgui_impl_sdl3` |

### 4.2 Audio 模块

| 当前文件 | 当前依赖 SFML | 接口化后依赖 | SDL3 实现类 |
|----------|---------------|-------------|------------|
| `Music.hpp` | `std::unique_ptr<sf::Music>` | `std::unique_ptr<IAudioSource>` | `SDL3MusicSource` |
| `SFX.hpp` | `std::unique_ptr<sf::Sound>` | `std::unique_ptr<IAudioSource>` | `SDL3SFXSource` |
| `SFXManager.hpp` | `std::unique_ptr<sf::SoundBuffer>` | `std::unique_ptr<IAudioBuffer>` | `SDL3AudioBuffer` |
| `VolumeManager.hpp` | 无依赖（纯数值） | 无需改变 | 无需改变 |

### 4.3 Media 模块

| 当前文件 | 当前依赖 SFML | 接口化后依赖 | SDL3 实现类 |
|----------|---------------|-------------|------------|
| `FFmpegPlayback` | `sf::Texture::update()` | `ITexture::Update()` | `SDL3MediaDecoder` + `SDL3Texture` |

### 4.4 Entity 模块

| 当前文件 | 当前依赖 SFML | 接口化后依赖 | SDL3 实现类 |
|----------|---------------|-------------|------------|
| `Entity.hpp` | `sf::Texture`, `sf::CircleShape`, `sf::Color`, `sf::Vector2f`, `sf::RenderWindow` | `ITexture`, `Color`, `Vec2`, `IRenderTarget&` | `SDL3Texture` |
| `Entity::Draw()` | `sf::RenderWindow& window` | `IRenderTarget& target` | 通过 target 绘制 |
| 圆形绘制 | `sf::CircleShape` | 纹理缓存 + `SDL_RenderTexture` | 预生成圆形纹理 |

---

## 5. 圆形绘制方案（重点问题）

SDL3 没有 `sf::CircleShape` 的开箱即用等效物。推荐方案：运行时生成圆形纹理缓存。

```cpp
// 在 SDL3RenderWindow 中提供圆形纹理生成
auto SDL3RenderWindow::GenerateCircleTexture(float radius) -> SDL_Texture* {
    int diameter = static_cast<int>(radius * 2);
    SDL_Texture* texture = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        diameter, diameter
    );

    SDL_SetRenderTarget(renderer_, texture);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    for (int y = -static_cast<int>(radius); y <= static_cast<int>(radius); y++) {
        for (int x = -static_cast<int>(radius); x <= static_cast<int>(radius); x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderPoint(renderer_, x + radius, y + radius);
            }
        }
    }

    SDL_SetRenderTarget(renderer_, nullptr);
    return texture;
}

// 使用纹理缓存绘制
auto Entity::Draw(IRenderTarget& target) -> void {
    // target 内部使用 SDL_RenderTexture 渲染圆形纹理
    target.DrawCircle(GetPosition(), GetRadius(), color_);
}
```

---

## 6. 文件目录结构（迁移后最终状态）

```
迁移前 (SFML 原生)             迁移后 (SDL3 原生)
─────────────────             ────────────────
Engine/                       Engine/
├── Interfaces/          新增 ├── Interfaces/
│   IRenderTarget.hpp          │   IRenderTarget.hpp
│   ITexture.hpp               │   ITexture.hpp
│   IRenderWindow.hpp          │   IRenderWindow.hpp
│   IAudioBuffer.hpp           │   IAudioBuffer.hpp
│   IAudioSource.hpp           │   IAudioSource.hpp
│   ISFXManager.hpp            │   ISFXManager.hpp
│   IMediaDecoder.hpp          │   IMediaDecoder.hpp
├── Render/              新增 ├── Render/
│   (SFML/ + SDL3/)           │   └── SDL3RenderWindow.hpp/cpp
│                              │   └── SDL3Texture.hpp/cpp
Window/                       Window/
├── RenderWindow.hpp/cpp  SFML ├── RenderWindow.hpp/cpp  接口
├── Screen.hpp            SFML ├── Screen.hpp            接口
├── Manager/ScreenManager.hpp  ├── Manager/ScreenManager.hpp
└── Debugger.hpp/cpp      SFML └── Debugger.hpp/cpp      接口+ImGui SDL3

Media/Audio/                  Media/Audio/
├── Music/Music.hpp/cpp  SFML ├── Music/Music.hpp/cpp    接口
├── SFX/SFX.hpp/cpp      SFML ├── SFX/SFX.hpp/cpp        接口
├── SFX/Manager/SFXManager.hpp ├── SFX/Manager/SFXManager.hpp 接口
├── Manager/VolumeManager.hpp  ├── Manager/VolumeManager.hpp
│                              ├── SDL3AudioSource.hpp/cpp  新增
│                              ├── SDL3AudioBuffer.hpp/cpp  新增

Components/Entities/          Components/Entities/
├── Entity.hpp/cpp        SFML ├── Entity.hpp/cpp          接口
├── Player.hpp/NPC.hpp    SFML ├── Player.hpp/NPC.hpp      接口

ThirdParty/Lib/               ThirdParty/Lib/
├── SFML-3.0.0/          删除──→  (已移除)
├── ImGUI-SFML/          删除──→  (已移除)
├── lua-5.4.7/                 ├── lua-5.4.7/
├── ffmpeg-7.1/                ├── ffmpeg-7.1/
                               ├── SDL3/               新增
                               ├── imgui/              新增(含SDL3后端)
                               └── dr_mp3 / libvorbis  新增(音频解码)
```

---

## 7. CMakeLists.txt 变更

```cmake
# ───── 新增：接口库（纯头文件） ─────
add_library(engine_interfaces INTERFACE)
target_include_directories(engine_interfaces INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Interfaces
)

# ───── 新增：SDL3 渲染后端 ─────
find_package(SDL3 REQUIRED)
add_library(engine_render_sdl3 STATIC
    Engine/Render/SDL3RenderWindow.cpp
    Engine/Render/SDL3Texture.cpp
)
target_link_libraries(engine_render_sdl3 PUBLIC
    engine_interfaces
    SDL3::SDL3
)

# ───── 新增：SDL3 音频后端 ─────
add_library(engine_audio_sdl3 STATIC
    Media/Audio/SDL3AudioSource.cpp
    Media/Audio/SDL3AudioBuffer.cpp
)
target_link_libraries(engine_audio_sdl3 PUBLIC
    engine_interfaces
    SDL3::SDL3
)

# ───── 引擎模块改为依赖接口 ─────
target_link_libraries(engine_windows PUBLIC
    engine_interfaces
    engine_render_sdl3
)
target_link_libraries(engine_media PUBLIC
    engine_interfaces
    engine_audio_sdl3
)
target_link_libraries(engine_entities PUBLIC
    engine_interfaces
)

# ───── Example 移除 sfml_all ─────
target_link_libraries(example_simple_window PUBLIC
    engine_windows
    SDL3::SDL3  # 仅 ImGui 等平台相关代码需要
)

# ───── 移除所有 SFML 相关 ─────
# set(SFML_DIR "...")               -- 删除
# set(IMGUI_SFML_DIR "...")         -- 删除
# find_package(SFML 3.0 REQUIRED)   -- 删除
# sfml_all 链接                      -- 全部删除
```

---

## 8. 迁移阶段与时间线

### 阶段 0：环境准备（3-5 天）

| 任务 | 产出 |
|------|------|
| 下载 SDL3、Dear ImGui SDL3 后端 | ThirdParty 目录更新 |
| 选择音频解码器（推荐 `dr_mp3` + `dr_wav`，单头文件零依赖） | 解码器依赖确认 |
| 配置 CMake 新查找路径 | SDL3 可编译验证 |

### 阶段 1：定义接口 + SFML 适配层（5-7 天）

| 任务 | 说明 |
|------|------|
| 创建 `Engine/Interfaces/` 全部接口头文件 | IRenderTarget, ITexture, IRenderWindow, IAudioBuffer, IAudioSource, ISFXManager, IMediaDecoder |
| 定义辅助类型 | Color, IEvent, EventType, Vec2, Rect |
| 编写 SFML 适配层 | `SFMLRenderWindow`, `SFMLTexture`, `SFMLAudioSource` 等 |

### 阶段 2：引擎模块接口化（7-10 天）

| 顺序 | 模块 | 变更 |
|------|------|------|
| 1 | `RenderWindow` | 持有 `IRenderWindow*`，主循环通过接口调用 |
| 2 | `Screen` / `ScreenManager` | 签名替换为 `IRenderTarget&` / `const IEvent&` |
| 3 | `Entity` / `Player` / `NPC` | `sf::Texture` → `ITexture*`，`Draw` 用 `IRenderTarget&` |
| 4 | `Music` | `std::unique_ptr<sf::Music>` → `std::unique_ptr<IAudioSource>` |
| 5 | `SFX` / `SFXManager` | `sf::Sound` / `sf::SoundBuffer` → `IAudioSource` / `IAudioBuffer` |
| 6 | `Debugger` | 分离 ImGui 后端，通过 `GetNativeHandle()` 初始化 |
| 7 | Lua 绑定 | 更新 Lua 侧的 SFML 引用为接口调用 |
| 8 | Example 代码 | 全部重新编译验证 |

> 此阶段完成后，引擎模块不再直接依赖 SFML，所有 SFML 依赖集中在适配层文件中。

### 阶段 3：编写 SDL3 实现（10-14 天）

| 模块 | 实现类 | 依赖 |
|------|--------|------|
| 窗口/渲染 | `SDL3RenderWindow`, `SDL3Texture` | SDL3 |
| 音频 | `SDL3AudioSource`, `SDL3AudioBuffer` | SDL3 + dr_mp3/dr_wav |
| 媒体解码 | `SDL3MediaDecoder` | FFmpeg（不变）+ SDL3 |
| 圆形纹理 | `GenerateCircleTexture()` | SDL3 |

### 阶段 4：切换 + 清理（3-5 天）

| 任务 | 说明 |
|------|------|
| CMake 切到 `engine_render_sdl3` / `engine_audio_sdl3` | 替换 SFML 适配层 |
| 删除 SFML 适配层文件 | `Engine/Render/SFML/*`, `Audio/Interface/SFML*` |
| 删除 ThirdParty SFML | 移除 `SFML-3.0.0`, `ImGUI-SFML` 目录 |
| 删除 CMake SFML 配置 | DLL copy、find_package 等 |
| 全面编译 + 运行 Example | 确认全部正常 |

**总计：约 4-6 周（全职）**

---

## 9. 关键技术风险与应对

| 风险 | 等级 | 应对 |
|------|------|------|
| **sf::Music 流式解码** | 🔴 高 | 推荐 `dr_mp3` / `dr_wav`（单 .h 文件，零依赖集成），线程安全解码推流 |
| **sf::CircleShape 缺失** | 🟡 中 | 预生成圆形纹理缓存，运行时缩放重用，性能几乎无损 |
| **音频时序差异** | 🟡 中 | SFML 自动管理音频线程；SDL3 需要手动推流 |
| **SDL3 API 变更** | 🟢 低 | SDL 3.4.0 已发布稳定版，锁定版本即可 |
| **ImGui 后端调试** | 🟢 低 | Dear ImGui 官方提供 `imgui_impl_sdl3`，社区验证充分 |

---

## 10. 迁移前后对比

| 维度 | 迁移前 (SFML) | 迁移后 (SDL3) |
|------|-------------|-------------|
| 架构 | 引擎模块直接依赖 SFML 类型 | 引擎模块依赖接口，不感知底层 |
| SFML 耦合文件 | 20 个 | 0 个（SDL3 实现文件替代） |
| 渲染原语 | `sf::CircleShape`、`sf::Sprite` | 圆形纹理缓存 + `SDL_RenderTexture` |
| 音频流 | `sf::Music` 内置解码 | `SDL_AudioStream` + 外部解码器 |
| 事件风格 | `std::optional` + `event->is<>()` | 统一 `IEvent` 结构体 |
| PNG 加载 | `sf::Texture::loadFromFile` | `SDL_LoadPNG` (SDL3.4+ 内置) |
| 用户代码 | 必须包含 `<SFML/Graphics.hpp>` | 只包含 `IRenderTarget.hpp` 等接口头文件 |
| 可测试性 | 需 SFML 环境 | 可用 Mock 对象替代 SDL3 实现进行单元测试 |
| 跨平台 | Windows/Linux/macOS/Android | + Steam Deck / iOS / 游戏主机 |

---

## 11. 总结

本方案采用**接口层 + SDL3 唯一实现**策略：

- **抽象接口层永久保留** — 引擎模块依赖 `IRenderTarget`、`IAudioSource` 等接口，不感知底层库
- **SDL3 是唯一后端** — 迁移完成后 SFML 代码全部移除，不保留多后端切换
- **迁移路径清晰** — 定义接口 → 引擎模块接口化 → 编写 SDL3 实现 → 切换 + 清理
- **用户代码不依赖 SDL3** — SDK 用户只需包含接口头文件

```
迁移完成后：
┌─────────────────────────────────────┐
│         用户代码 (Game)               │
│     Screen 子类 · Entity 子类         │
├─────────────────────────────────────┤
│   引擎上层 (ScreenManager, Music,    │
│   SFX, Entity, Debugger, Lua)        │
│         ↕ 依赖纯虚接口               │
│   IRenderTarget / ITexture           │
│   IAudioSource / IAudioBuffer        │
│         ↕ 唯一实现                   │
│   SDL3RenderWindow / SDL3Texture     │
│   SDL3AudioSource / SDL3AudioBuffer  │
└─────────────────────────────────────┘
```

> **建议**：优先完成接口定义 + 引擎模块接口化（阶段 1-2），确保架构解耦后，再启动 SDL3 实现的编写。

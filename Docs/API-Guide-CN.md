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

**播放器/服务（用户按需创建）** — 直接构造实例：
- `SFX` — 音效播放
- `Music` — 音乐播放
- `MusicFade` — 音乐淡入淡出

---

## 日志系统

```cpp
// 直接使用宏
LOG_INFO(atom::LogChannel::ATOM_MAIN, "Engine started");
LOG_WARNING(atom::LogChannel::ATOM_FILESYSTEM, "File not found");
LOG_ERROR(atom::LogChannel::ATOM_LUA, "Script error");

// 设置日志显示级别
atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_WARNING);

// 自定义日志通道
const atom::LogChannel GAME_NPC("Game.NPC");
LOG_INFO(GAME_NPC, "NPC spawned");
```

---

## 窗口 + 屏幕管理

```cpp
#include <Window/RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Screen.hpp>

class MenuScreen : public atom::Screen {
    void Render(sf::RenderWindow& window) override { /* ... */ }
    bool HandleEvent(const sf::Event& event) override { /* ... */ }
    void Update(float deltaTime) override { /* ... */ }
};

// ScreenManager 是全局单例
atom::ScreenManager::GetInstance().LoadScreen("menu", std::make_unique<MenuScreen>());
atom::ScreenManager::GetInstance().SwitchScreen("menu");

// RenderWindow 也是全局单例
auto& window = atom::RenderWindow::GetInstance();
window.Initialize("My Game", atom::Vec2{1280, 720});
window.Run();   // 无需传参，内部使用 ScreenManager 单例
```

---

## 音效

```cpp
#include <Media/Audio/SFX/SFX.hpp>

// SFX 是普通实例，可创建多个
atom::SFX sfx;
sfx.Load("explosion", "assets/exp.wav");
sfx.Play("explosion");   // 播放时自动读取 VolumeManager 的全局音量

// 内部使用全局 SFXManager 管理音频缓冲
```

---

## 音乐 + 淡入淡出

```cpp
#include <Media/Audio/Music/Music.hpp>
#include <Media/Audio/Plugs/MusicFade.hpp>

atom::Music music;
music.Load("bgm", "assets/bgm.ogg");
music.Play("bgm");        // 播放时自动读取 VolumeManager 的全局音量

// MusicFade 需要 Music 实例
atom::audio::MusicFade fade{music};
fade.Switch("bgm2", 2.0f);
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
    void OnDrawOverlay() override {
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", fps);
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

// Music 和 SFX 是实例，需要注册给 Lua
atom::Music music;
atom::SFX sfx;

SetLuaMusicInstance(music);
SetLuaSFXInstance(sfx);

// SFXManager、VolumeManager 是全局单例，Lua 直接访问

LuaLoader lua;
lua.Initialize();

// Lua 中调用：
// Music:Load("bgm", "bgm.ogg")
// Music:Play("bgm")
// SFX:Load("exp", "exp.wav")
// SFX:Play("exp")
// SFXManager:LoadSFXFiles("id", "path")
// VolumeManager.SetMasterVolume(80)
// VolumeManager.SetSfxVolume(100)
// VolumeManager.SetMusicVolume(80)
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
    music.Load("bgm", "bgm.ogg");
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
| `Log` | `Log::GetLogInstance()` | 全局日志系统 |
| `RenderWindow` | `RenderWindow::GetInstance()` | 唯一的渲染窗口 |
| `ScreenManager` | `ScreenManager::GetInstance()` | 全局屏幕管理，`Run()` 内部自动引用 |
| `SFXManager` | `SFXManager::GetManager()` | 全局音频缓冲缓存 |
| `VolumeManager` | `VolumeManager::GetInstance()` | 全局音量（总音量 + 分类音量） |
| `SFX` | `SFX()` 构造实例 | 音效播放器，可创建多个 |
| `Music` | `Music()` 构造实例 | 音乐播放器 |
| `MusicFade` | `MusicFade(Music&)` | 淡入淡出，需要 Music 引用 |
| `Debugger` | `Debugger()` + `Attach(window)` | 调试覆盖层 |

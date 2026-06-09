# Audio Plugin System - Design Proposal

## 概述

将 MusicFade 等效果器改造为统一的插件体系，挂载到 Music（未来可扩展到 SFX）上。

---

## 核心接口

### 插件基类

```cpp
// Media/Audio/Plugs/AudioPlugin.hpp
namespace atom::audio {
    class AudioPlugin {
    public:
        virtual ~AudioPlugin() = default;

        // Called every frame by the host
        virtual auto Process(float dt) -> void = 0;

        // Receive events from the host or other plugins
        virtual auto OnEvent(const AudioEvent& event) -> void {}

        virtual auto GetName() const -> std::string = 0;

        virtual auto IsActive() const -> bool { return true; }
        virtual auto SetActive(bool active) -> void {}
    };
}
```

### 事件系统

```cpp
// Media/Audio/Plugs/AudioEvent.hpp
namespace atom::audio {
    enum class EventType {
        PlaybackStarted,
        PlaybackStopped,
        TrackSwitched,
        VolumeChanged,
        PluginActivated,
    };

    struct AudioEvent {
        EventType type;
        std::string track_id;
        float value = 0.0f;
        void* custom_data = nullptr;
    };
}
```

---

## 宿主集成

Music 持有插件列表，每帧依次调用 `Process(dt)`，关键事件发生后调用 `BroadcastEvent()`。

```cpp
class Music {
private:
    std::vector<std::unique_ptr<AudioPlugin>> plugins_;

    auto BroadcastEvent(const AudioEvent& event) -> void {
        for (auto& plugin : plugins_) {
            plugin->OnEvent(event);
        }
    }

public:
    auto AttachPlugin(std::unique_ptr<AudioPlugin> plugin) -> void {
        plugins_.push_back(std::move(plugin));
    }

    auto UpdatePlugins(float dt) -> void {
        for (auto& plugin : plugins_) {
            if (plugin->IsActive()) {
                plugin->Process(dt);
            }
        }
    }
};
```

---

## MusicFade 改造示例

去掉独立线程，改为帧驱动：

```cpp
class MusicFade final : public AudioPlugin {
    Music& music_;
    FadeState state_ = FadeState::Idle;
    std::string from_id_, to_id_;
    float duration_ = 0.0f;
    float progress_ = 0.0f;

    auto GetName() const -> std::string override {
        return "MusicFade";
    }

    auto Process(float dt) -> void override {
        if (state_ == FadeState::Idle) return;

        progress_ += dt / duration_;

        if (state_ == FadeState::FadingOut) {
            float vol = peak_volume_ * (1.0f - progress_);
            music_.SetVolume(from_id_, vol);
            if (progress_ >= 1.0f) {
                // 切换曲目
                music_.Stop(from_id_);
                music_.Play(to_id_, 0.0f);
                state_ = FadeState::FadingIn;
                progress_ = 0.0f;
            }
        } else if (state_ == FadeState::FadingIn) {
            float vol = peak_volume_ * progress_;
            music_.SetVolume(to_id_, vol);
            if (progress_ >= 1.0f) {
                state_ = FadeState::Idle;
                // 通知其他插件曲目已切换
                // BroadcastEvent({EventType::TrackSwitched, to_id_});
            }
        }
    }
};
```

---

## 未来可实现的插件

| 插件 | 说明 | 作用于 |
|------|------|--------|
| MusicFade | 淡入淡出切换曲目 | Music |
| HighCutFilter | 高切低通滤波器 | Music, SFX |
| LowCutFilter | 低切高通滤波器 | Music, SFX |
| GlitchEffect | 故障/卡碟效果 | Music, SFX |
| ReverbEffect | 混响效果 | SFX |
| DelayEffect | 延迟/回声效果 | SFX |
| Equalizer | 多段均衡器 | Music |

---

## 注意事项

- 插件按 `Process(dt)` 调用顺序执行，顺序影响最终效果
- 插件的 `OnEvent()` 在宿主广播时同步调用，不要在事件处理中做耗时操作
- 插件可以持有 `Music&` 或 `SFX&` 引用来控制播放，但应避免直接操作内部数据
- 未来可以将 Music 和 SFX 的公共接口抽象为 `AudioHost`，实现插件对两者的统一支持

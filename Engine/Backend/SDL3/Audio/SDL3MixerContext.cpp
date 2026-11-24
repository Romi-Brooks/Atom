#include "SDL3MixerContext.hpp"

#include <mutex>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <Log/LogSystem.hpp>

namespace atom::backend::sdl3mixer {

auto SDL3MixerContext::Acquire() -> std::shared_ptr<SDL3MixerContext> {
    static std::mutex context_mutex;
    static std::weak_ptr<SDL3MixerContext> cached;

    std::scoped_lock lock{context_mutex};
    if (auto existing = cached.lock()) {
        LOG_DEBUG(atom::log::backend::Audio::sdl3_mixer, "Reusing the active SDL3_mixer context");
        return existing;
    }

    auto context = std::shared_ptr<SDL3MixerContext>{new SDL3MixerContext()};
    if (context->IsReady()) {
        cached = context;
    }
    return context;
}

SDL3MixerContext::SDL3MixerContext() {
    initialized_ = MIX_Init();
    if (!initialized_) {
        LOG_ERROR(atom::log::backend::Audio::sdl3_mixer, "MIX_Init failed: " + std::string(SDL_GetError()));
        return;
    }
    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer_) {
        LOG_ERROR(atom::log::backend::Audio::sdl3_mixer,
                  "MIX_CreateMixerDevice failed: " + std::string(SDL_GetError()));
        MIX_Quit();
        initialized_ = false;
    } else {
        LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "playback device and mixer initialized");
    }
}

SDL3MixerContext::~SDL3MixerContext() {
    LOG_INFO(atom::log::backend::Audio::sdl3_mixer, "mixer shutdown");
    // Idempotent: both handles are cleared so a repeated teardown cannot
    // double-destroy the mixer or unbalance the MIX_Init()/MIX_Quit() refcount.
    if (mixer_) {
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }
    if (initialized_) {
        MIX_Quit();
        initialized_ = false;
    }
}

auto SDL3MixerContext::IsReady() const -> bool {
    return mixer_ != nullptr;
}

auto SDL3MixerContext::Mixer() const -> MIX_Mixer* {
    return mixer_;
}

} // namespace atom::backend::sdl3mixer

#include "SDLCore.hpp"

#include <SDL3/SDL.h>

namespace atom {

std::atomic<uint32_t> SDLCore::ref_count_{0};
SDLCore::Config SDLCore::active_config_{};

auto SDLCore::Initialize(const Config& cfg) -> bool {
    if (ref_count_.fetch_add(1) > 0) {
        // Already initialized; the first caller's config wins.
        return true;
    }

    active_config_ = cfg;

    uint32_t flags = 0;
    if (cfg.video)  flags |= SDL_INIT_VIDEO;
    if (cfg.audio)  flags |= SDL_INIT_AUDIO;
    if (cfg.events) flags |= SDL_INIT_EVENTS;

    if (!SDL_Init(flags)) {
        ref_count_.fetch_sub(1);
        return false;
    }
    return true;
}

auto SDLCore::Shutdown() -> void {
    if (ref_count_.fetch_sub(1) == 1) {
        SDL_Quit();
    }
}

auto SDLCore::RefCount() -> uint32_t {
    return ref_count_.load();
}

} // namespace atom

#ifndef ATOM_SDL3_MIXER_CONTEXT_HPP
#define ATOM_SDL3_MIXER_CONTEXT_HPP

#include <memory>

struct MIX_Mixer;

namespace atom::backend::sdl3mixer {

// Process-wide SDL_mixer context.
//
// MIX_Init()/MIX_Quit() are process-global (MIX_Quit tears down every mixer,
// audio decoder and audio object that still exists), so a per-backend-instance
// context would make two seconds of overlap -- exactly what a backend switch
// creates -- fight over the same global state. Every backend instance therefore
// acquires the same shared context through Acquire(); the context is destroyed
// when the last backend and the last source release it.
//
// A failed context is not cached, so a later attempt can retry once the audio
// environment is usable again.
class SDL3MixerContext final {
    public:
        [[nodiscard]] static auto Acquire() -> std::shared_ptr<SDL3MixerContext>;

        ~SDL3MixerContext();

        SDL3MixerContext(const SDL3MixerContext&) = delete;
        auto operator=(const SDL3MixerContext&) -> SDL3MixerContext& = delete;
        SDL3MixerContext(SDL3MixerContext&&) = delete;
        auto operator=(SDL3MixerContext&&) -> SDL3MixerContext& = delete;

        [[nodiscard]] auto IsReady() const -> bool;
        [[nodiscard]] auto Mixer() const -> MIX_Mixer*;

    private:
        SDL3MixerContext();

        MIX_Mixer* mixer_ = nullptr;
        bool initialized_ = false;
};

} // namespace atom::backend::sdl3mixer

#endif // ATOM_SDL3_MIXER_CONTEXT_HPP

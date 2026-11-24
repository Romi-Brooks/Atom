#include "SDL3MixerAudioBackend.hpp"

#include <SDL3/SDL.h>

#include "SDL3MixerContext.hpp"
#include "SDL3MixerSource.hpp"
#include "SDL3MixerStreamingSource.hpp"
#include <Log/LogSystem.hpp>

namespace atom::backend::sdl3mixer {

namespace {
auto ToSpec(const atom::audio::AudioSpec& spec) -> SDL_AudioSpec {
    SDL_AudioFormat format = SDL_AUDIO_S16;
    switch (spec.format) {
    case atom::audio::AudioSampleFormat::Unsigned8:
        format = SDL_AUDIO_U8;
        break;
    case atom::audio::AudioSampleFormat::Signed16:
        format = SDL_AUDIO_S16;
        break;
    case atom::audio::AudioSampleFormat::Signed32:
        format = SDL_AUDIO_S32;
        break;
    case atom::audio::AudioSampleFormat::Float32:
        format = SDL_AUDIO_F32;
        break;
    }
    SDL_AudioSpec result{};
    result.format = format;
    result.channels = static_cast<int>(spec.channels);
    result.freq = static_cast<int>(spec.sample_rate);
    return result;
}
} // namespace

SDL3MixerAudioBackend::SDL3MixerAudioBackend() : context_(SDL3MixerContext::Acquire()) {}

auto SDL3MixerAudioBackend::CreateMusicSource(std::vector<uint8_t> pcm, const atom::audio::AudioSpec& spec)
    -> std::unique_ptr<atom::audio::IAudioSource> {
    auto source = std::make_unique<SDL3MixerSource>(context_, pcm.data(), pcm.size(), ToSpec(spec));
    if (!source->IsValid())
        return nullptr;
    source->BindRegistry(sources_);
    return source;
}

auto SDL3MixerAudioBackend::CreateSFXSource(const std::vector<uint8_t>& pcm, const atom::audio::AudioSpec& spec)
    -> std::unique_ptr<atom::audio::IAudioSource> {
    auto source = std::make_unique<SDL3MixerSource>(context_, pcm.data(), pcm.size(), ToSpec(spec));
    if (!source->IsValid())
        return nullptr;
    source->BindRegistry(sources_);
    return source;
}

auto SDL3MixerAudioBackend::CreateStreamingMusicSource(std::unique_ptr<atom::audio::IAudioDecoder> decoder,
                                                       const atom::audio::AudioSpec& spec)
    -> std::unique_ptr<atom::audio::IAudioSource> {
    auto source = std::make_unique<SDL3MixerStreamingSource>(context_, std::move(decoder), ToSpec(spec));
    if (!source->IsValid())
        return nullptr;
    source->BindRegistry(sources_);
    return source;
}

auto SDL3MixerAudioBackend::Quiesce() -> void {
    const auto tracked = sources_.TrackedCount();
    if (tracked > 0) {
        LOG_INFO(atom::log::backend::Audio::sdl3_mixer,
                 "Detaching " + std::to_string(tracked) + " live source(s) before backend teardown");
    }
    sources_.DetachAll();
}

auto SDL3MixerAudioBackend::IsReady() const -> bool {
    return audio_runtime_.IsValid() && context_ && context_->IsReady();
}

} // namespace atom::backend::sdl3mixer

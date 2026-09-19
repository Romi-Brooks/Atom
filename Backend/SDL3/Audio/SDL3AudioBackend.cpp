#include "SDL3AudioBackend.hpp"

#include <SDL3/SDL.h>
#include <Backend/SDL3/Audio/SDL3MusicSource.hpp>
#include <Backend/SDL3/Audio/SDL3SFXSource.hpp>
#include <Backend/SDL3/Audio/SDL3StreamingMusicSource.hpp>
#include <Log/LogSystem.hpp>

namespace atom::backend::sdl3 {

namespace {
auto ToSDLSpec(const atom::audio::AudioSpec& spec) -> SDL_AudioSpec {
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

SDL3AudioBackend::SDL3AudioBackend() {
    if (audio_runtime_.IsValid()) {
        LOG_INFO(atom::log::backend::Audio::sdl3, "Native SDL3 audio backend initialized");
    } else {
        LOG_ERROR(atom::log::backend::Audio::sdl3, "Failed to initialize native SDL3 audio backend");
    }
}

auto SDL3AudioBackend::CreateMusicSource(std::vector<uint8_t> pcm, const atom::audio::AudioSpec& spec)
    -> std::unique_ptr<atom::audio::IAudioSource> {
    if (!audio_runtime_.IsValid())
        return nullptr;
    LOG_INFO(atom::log::backend::Audio::sdl3, "Creating buffered music source");
    auto source = std::make_unique<SDL3MusicSource>(std::move(pcm), ToSDLSpec(spec));
    source->BindRegistry(sources_);
    return source;
}

auto SDL3AudioBackend::CreateStreamingMusicSource(std::unique_ptr<atom::audio::IAudioDecoder> decoder,
                                                  const atom::audio::AudioSpec& spec)
    -> std::unique_ptr<atom::audio::IAudioSource> {
    if (!audio_runtime_.IsValid())
        return nullptr;
    if (!decoder || !decoder->IsOpen())
        return nullptr;
    LOG_INFO(atom::log::backend::Audio::sdl3, "Creating streaming music source from initialized decoder");
    auto source = std::make_unique<SDL3StreamingMusicSource>(std::move(decoder), ToSDLSpec(spec));
    source->BindRegistry(sources_);
    return source;
}

auto SDL3AudioBackend::CreateSFXSource(const std::vector<uint8_t>& pcm, const atom::audio::AudioSpec& spec)
    -> std::unique_ptr<atom::audio::IAudioSource> {
    if (!audio_runtime_.IsValid())
        return nullptr;
    LOG_INFO(atom::log::backend::Audio::sdl3, "Creating SFX source");
    auto source = std::make_unique<SDL3SFXSource>();
    source->SetBuffer(pcm.data(), static_cast<uint32_t>(pcm.size()));
    source->SetSpec(ToSDLSpec(spec));
    source->BindRegistry(sources_);
    return source;
}

auto SDL3AudioBackend::Quiesce() -> void {
    const auto tracked = sources_.TrackedCount();
    if (tracked > 0) {
        LOG_INFO(atom::log::backend::Audio::sdl3,
                 "Detaching " + std::to_string(tracked) + " live source(s) before backend teardown");
    }
    sources_.DetachAll();
}

auto SDL3AudioBackend::IsReady() const -> bool {
    return audio_runtime_.IsValid();
}

} // namespace atom::backend::sdl3

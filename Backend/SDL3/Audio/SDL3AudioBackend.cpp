#include "SDL3AudioBackend.hpp"

#include <SDL3/SDL.h>
#include <Backend/SDL3/Audio/SDL3MusicSource.hpp>
#include <Backend/SDL3/Audio/SDL3SFXSource.hpp>
#include <Backend/SDL3/Audio/SDL3StreamingMusicSource.hpp>

namespace atom {
namespace {
auto ToSDLSpec(const AudioSpec& spec) -> SDL_AudioSpec {
    SDL_AudioFormat format = SDL_AUDIO_S16;
    switch (spec.format) {
    case AudioSampleFormat::Unsigned8:
        format = SDL_AUDIO_U8;
        break;
    case AudioSampleFormat::Signed16:
        format = SDL_AUDIO_S16;
        break;
    case AudioSampleFormat::Signed32:
        format = SDL_AUDIO_S32;
        break;
    case AudioSampleFormat::Float32:
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

auto SDL3AudioBackend::CreateMusicSource(std::vector<uint8_t> pcm, const AudioSpec& spec)
    -> std::unique_ptr<IAudioSource> {
    if (!audio_runtime_.IsValid())
        return nullptr;
    return std::make_unique<SDL3MusicSource>(std::move(pcm), ToSDLSpec(spec));
}

auto SDL3AudioBackend::CreateStreamingMusicSource(std::unique_ptr<IAudioDecoder> decoder, const AudioSpec& spec)
    -> std::unique_ptr<IAudioSource> {
    if (!audio_runtime_.IsValid())
        return nullptr;
    if (!decoder || !decoder->IsOpen())
        return nullptr;
    return std::make_unique<SDL3StreamingMusicSource>(std::move(decoder), ToSDLSpec(spec));
}

auto SDL3AudioBackend::CreateSFXSource(const std::vector<uint8_t>& pcm, const AudioSpec& spec)
    -> std::unique_ptr<IAudioSource> {
    if (!audio_runtime_.IsValid())
        return nullptr;
    auto source = std::make_unique<SDL3SFXSource>();
    source->SetBuffer(pcm.data(), static_cast<uint32_t>(pcm.size()));
    source->SetSpec(ToSDLSpec(spec));
    return source;
}

auto SDL3AudioBackend::IsReady() const -> bool {
    return audio_runtime_.IsValid();
}
} // namespace atom

#include "BackendRuntime.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

#include <Backend/Audio/Decoder/minimp3/Minimp3Decoder.hpp>
#include <Backend/Audio/Decoder/SDL3Wav/SDL3WavDecoder.hpp>
#include <Backend/Audio/Decoder/WavProf/WavProfDecoder.hpp>
#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Contracts/Audio/NullAudioBackend.hpp>
#include <Backend/Runtime/IAudioBackendChangeListener.hpp>
#include <Backend/SDL3/Audio/SDL3AudioBackend.hpp>
#include <Backend/SDL3/Audio/SDL3MixerAudioBackend.hpp>

#include <Log/LogSystem.hpp>

namespace atom::backend {

namespace {
auto NormalizeBackendId(const std::string_view id) -> std::string {
    std::string normalized{id};
    std::ranges::transform(normalized, normalized.begin(),
                           [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return normalized;
}
} // namespace

auto BackendRuntime::GetInstance() -> BackendRuntime& {
    static BackendRuntime instance;
    return instance;
}

BackendRuntime::BackendRuntime() {
    RegisterAvailableBackends();
    null_backend_ = std::make_shared<audio::NullAudioBackend>();
    const auto default_audio_id = ToString(AudioBackendId::Sdl3);
    LOG_INFO(atom::log::backend::Runtime, "Initializing default audio backend '" + std::string{default_audio_id} + "'");
    audio_backend_ = registry_.CreateAudioBackend(default_audio_id);
    if (!audio_backend_)
        throw std::runtime_error("Failed to initialize default SDL3 audio backend");
    audio_backend_id_ = default_audio_id;
    LOG_INFO(atom::log::backend::Runtime, "Default audio backend '" + audio_backend_id_ + "' initialized");
    RegisterDefaultAudioDecoders(audio_decoders_);
}

BackendRuntime::~BackendRuntime() = default;

auto BackendRuntime::RegisterAvailableBackends() -> void {
    registry_.RegisterAudioBackend(std::string{ToString(AudioBackendId::Sdl3)},
                                   []() -> std::unique_ptr<audio::IAudioBackend> {
                                       auto backend = std::make_unique<sdl3::SDL3AudioBackend>();
                                       if (!backend->IsReady())
                                           return nullptr;
                                       return backend;
                                   });
    registry_.RegisterAudioBackend(std::string{ToString(AudioBackendId::Sdl3Mixer)},
                                   []() -> std::unique_ptr<audio::IAudioBackend> {
                                       auto backend = std::make_unique<sdl3mixer::SDL3MixerAudioBackend>();
                                       if (!backend->IsReady())
                                           return nullptr;
                                       return backend;
                                   });
}

auto BackendRuntime::RegisterDefaultAudioDecoders(audio::AudioDecoderRegistry& decoders) -> void {
    // SDL3 itself provides no audio codecs; the engine ships one decoder per
    // format. Add new formats here as they are implemented.
    //
    // .wav holds two implementations:
    //   preferred: WavProf       - streams with a 64 KiB window, so a long track
    //                              never has to be resident; measured faster than
    //                              the SDL3 loader for a whole-file PCM read
    //                              (34 MB WAV: ~7.9 ms vs ~13.5 ms).
    //   fallback:  SDL3Wav       - SDL_LoadWAV decodes the whole file, which also
    //                              covers the encodings WavProf rejects (MS ADPCM,
    //                              IMA ADPCM, A-Law, mu-Law).
    // AudioClipLoader walks the chain, so plain PCM uses the streaming decoder and
    // a compressed WAV still plays. Swap the two lines once WavProf no longer
    // needs the fallback, or Replace(".wav", ...) to force one implementation.
    decoders.Register(".wav", audio_decoder::CreateWavProfDecoder, "WavProf");
    decoders.RegisterFallback(".wav", audio_decoder::CreateSDL3WavDecoder, "SDL3Wav");
    decoders.Register(".mp3", [] { return std::make_unique<audio_decoder::Minimp3Decoder>(); }, "Minimp3");
}

auto BackendRuntime::Audio() -> audio::IAudioBackend& {
    std::scoped_lock lock{backend_mutex_};
    if (audio_backend_) {
        return *audio_backend_;
    }
    if (!reported_missing_backend_) {
        LOG_ERROR(atom::log::backend::Runtime,
                  "Audio backend requested while none is active; returning the null backend");
        reported_missing_backend_ = true;
    }
    return *null_backend_;
}

auto BackendRuntime::TryAudio() -> audio::IAudioBackend* {
    std::scoped_lock lock{backend_mutex_};
    return audio_backend_.get();
}

auto BackendRuntime::AcquireAudioBackend() -> std::shared_ptr<audio::IAudioBackend> {
    std::scoped_lock lock{backend_mutex_};
    return audio_backend_;
}

auto BackendRuntime::AudioDecoders() -> audio::AudioDecoderRegistry& {
    return audio_decoders_;
}
auto BackendRuntime::Registry() -> BackendRegistry& {
    return registry_;
}

auto BackendRuntime::SetAudioBackend(const AudioBackendId id) -> bool {
    const auto normalized_id = NormalizeBackendId(ToString(id));
    if (normalized_id == audio_backend_id_) {
        LOG_DEBUG(atom::log::backend::Runtime,
                  "Audio backend '" + normalized_id + "' is already active, no switch needed");
        return true;
    }
    if (!registry_.ContainsAudioBackend(normalized_id)) {
        LOG_ERROR(atom::log::backend::Runtime, "Audio backend '" + normalized_id + "' is not registered");
        return false;
    }

    const auto previous_id = audio_backend_id_;
    LOG_INFO(atom::log::backend::Runtime,
             "Switching audio backend from '" + previous_id + "' to '" + normalized_id + "'");

    // 1. Build the replacement first. The active backend and every source it owns
    //    stay untouched, so a failed switch is a genuine no-op instead of a
    //    "restored" backend that no longer owns the sources still in flight.
    std::shared_ptr<audio::IAudioBackend> replacement = registry_.CreateAudioBackend(normalized_id);
    if (!replacement) {
        LOG_ERROR(atom::log::backend::Runtime, "Failed to create audio backend '" + normalized_id +
                                                   "', keeping '" + previous_id + "' active");
        return false;
    }

    // 2. Listeners release the ids/sources they own while the old backend is
    //    still alive, then the old backend detaches anything the listeners do not
    //    own (e.g. a source held directly by a screen). After this point no
    //    source references the outgoing backend's handles.
    NotifyAudioBackendChanging();
    if (const auto outgoing = AcquireAudioBackend()) {
        outgoing->Quiesce();
    }

    // 3. Swap, then release the old backend outside the lock. Doing it in this
    //    order means concurrent TryAudio()/Audio() callers never observe a window
    //    without an active backend, and any caller that already holds a strong
    //    reference (AcquireAudioBackend) keeps it alive until it is done.
    std::shared_ptr<audio::IAudioBackend> previous;
    {
        std::scoped_lock lock{backend_mutex_};
        previous = std::move(audio_backend_);
        audio_backend_ = std::move(replacement);
        audio_backend_id_ = normalized_id;
        ++audio_backend_generation_;
    }

    NotifyAudioBackendChanged();
    LOG_INFO(atom::log::backend::Runtime, "Audio backend switched to '" + normalized_id + "' (generation " +
                                              std::to_string(audio_backend_generation_) + ")");
    return true;
}

auto BackendRuntime::GetAudioBackendId() const -> const std::string& {
    return audio_backend_id_;
}

auto BackendRuntime::GetAudioBackendGeneration() const -> std::uint64_t {
    std::scoped_lock lock{backend_mutex_};
    return audio_backend_generation_;
}

auto BackendRuntime::AddAudioListener(IAudioBackendChangeListener& listener) -> void {
    if (std::ranges::find(audio_listeners_, &listener) == audio_listeners_.end()) {
        audio_listeners_.push_back(&listener);
    }
}

auto BackendRuntime::RemoveAudioListener(IAudioBackendChangeListener& listener) -> void {
    std::erase(audio_listeners_, &listener);
}

auto BackendRuntime::NotifyAudioBackendChanging() -> void {
    const auto listeners = audio_listeners_;
    for (auto* listener : listeners)
        if (listener)
            listener->OnAudioBackendChanging();
}

auto BackendRuntime::NotifyAudioBackendChanged() -> void {
    const auto listeners = audio_listeners_;
    for (auto* listener : listeners)
        if (listener)
            listener->OnAudioBackendChanged();
}

} // namespace atom::backend

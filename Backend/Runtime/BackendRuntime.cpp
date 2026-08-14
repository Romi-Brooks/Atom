#include "BackendRuntime.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <Backend/Builtin/Audio/RegisterBuiltinAudio.hpp>
#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Runtime/IAudioBackendChangeListener.hpp>
#include <Backend/SDL3/Audio/RegisterSDL3Audio.hpp>
#include <Backend/SDL3/Audio/SDL3AudioBackend.hpp>

#include <Log/LogSystem.hpp>

namespace atom {
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
    audio_backend_ = registry_.CreateAudioBackend("sdl3");
    if (!audio_backend_)
        throw std::runtime_error("Failed to initialize default SDL3 audio backend");
    audio_backend_id_ = "sdl3";
    if (!registry_.InstallAudioDecoderBackend("sdl3", audio_decoders_)) {
        throw std::runtime_error("Failed to initialize default SDL3 audio decoder backend");
    }
    audio_decoder_backend_id_ = "sdl3";
}

BackendRuntime::~BackendRuntime() = default;

auto BackendRuntime::RegisterAvailableBackends() -> void {
    registry_.RegisterAudioBackend("sdl3", []() -> std::unique_ptr<IAudioBackend> {
        auto backend = std::make_unique<SDL3AudioBackend>();
        if (!backend->IsReady())
            return nullptr;
        return backend;
    });
    registry_.RegisterAudioDecoderBackend(
        "sdl3", [](AudioDecoderRegistry& decoders) { return RegisterSDL3AudioDecoders(decoders); });
    registry_.RegisterAudioDecoderBackend(
        "builtin", [](AudioDecoderRegistry& decoders) { return RegisterBuiltinAudioDecoders(decoders); });
}

auto BackendRuntime::Audio() -> IAudioBackend& {
    if (!audio_backend_)
        throw std::runtime_error("No active audio backend");
    return *audio_backend_;
}
auto BackendRuntime::AudioDecoders() -> AudioDecoderRegistry& {
    return audio_decoders_;
}
auto BackendRuntime::Registry() -> BackendRegistry& {
    return registry_;
}

auto BackendRuntime::SetAudioBackend(const std::string_view id) -> bool {
    const auto normalized_id = NormalizeBackendId(id);
    if (normalized_id == audio_backend_id_) {
        LOG_DEBUG(atom::LogChannel::ATOM_BACKEND_RUNTIME,
                  "Audio backend '" + normalized_id + "' is already active, no switch needed");
        return true;
    }
    if (!registry_.ContainsAudioBackend(normalized_id)) {
        LOG_ERROR(atom::LogChannel::ATOM_BACKEND_RUNTIME, "Audio backend '" + normalized_id + "' is not registered");
        return false;
    }

    const auto previous_id = audio_backend_id_;
    LOG_INFO(atom::LogChannel::ATOM_BACKEND_RUNTIME,
             "Switching audio backend from '" + previous_id + "' to '" + normalized_id + "'");
    NotifyAudioBackendChanging();
    audio_backend_.reset();
    auto replacement = registry_.CreateAudioBackend(normalized_id);
    if (!replacement) {
        LOG_ERROR(atom::LogChannel::ATOM_BACKEND_RUNTIME, "Failed to create audio backend '" + normalized_id +
                                                              "', restoring previous backend '" + previous_id + "'");
        audio_backend_ = registry_.CreateAudioBackend(previous_id);
        if (!audio_backend_) {
            throw std::runtime_error("Audio backend switch failed and the previous backend could not be restored");
        }
        return false;
    }
    audio_backend_ = std::move(replacement);
    audio_backend_id_ = normalized_id;
    LOG_INFO(atom::LogChannel::ATOM_BACKEND_RUNTIME, "Audio backend switched to '" + normalized_id + "'");
    return true;
}

auto BackendRuntime::SetAudioDecoderBackend(const std::string_view id) -> bool {
    const auto normalized_id = NormalizeBackendId(id);
    if (normalized_id == audio_decoder_backend_id_) {
        LOG_DEBUG(atom::LogChannel::ATOM_BACKEND_RUNTIME,
                  "Audio decoder backend '" + normalized_id + "' is already active, no switch needed");
        return true;
    }
    if (!registry_.ContainsAudioDecoderBackend(normalized_id)) {
        LOG_ERROR(atom::LogChannel::ATOM_BACKEND_RUNTIME,
                  "Audio decoder backend '" + normalized_id + "' is not registered");
        return false;
    }

    const auto previous_id = audio_decoder_backend_id_;
    LOG_INFO(atom::LogChannel::ATOM_BACKEND_RUNTIME,
             "Switching audio decoder backend from '" + previous_id + "' to '" + normalized_id + "'");
    AudioDecoderRegistry replacement;
    if (!registry_.InstallAudioDecoderBackend(normalized_id, replacement)) {
        LOG_ERROR(atom::LogChannel::ATOM_BACKEND_RUNTIME, "Failed to install audio decoder backend '" + normalized_id +
                                                              "', keeping previous backend '" + previous_id + "'");
        return false;
    }
    NotifyAudioDecoderBackendChanging();
    audio_decoders_ = std::move(replacement);
    audio_decoder_backend_id_ = normalized_id;
    LOG_INFO(atom::LogChannel::ATOM_BACKEND_RUNTIME, "Audio decoder backend switched to '" + normalized_id + "'");
    return true;
}

auto BackendRuntime::GetAudioBackendId() const -> const std::string& {
    return audio_backend_id_;
}

auto BackendRuntime::GetAudioDecoderBackendId() const -> const std::string& {
    return audio_decoder_backend_id_;
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

auto BackendRuntime::NotifyAudioDecoderBackendChanging() -> void {
    const auto listeners = audio_listeners_;
    for (auto* listener : listeners)
        if (listener)
            listener->OnAudioDecoderBackendChanging();
}

} // namespace atom

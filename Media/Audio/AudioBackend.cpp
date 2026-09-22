// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "AudioBackend.hpp"

#include <Backend/Runtime/BackendRuntime.hpp>

namespace atom::audio {

auto GetAudioBackendId() -> const std::string& {
    return backend::BackendRuntime::GetInstance().GetAudioBackendId();
}

auto GetAudioBackendGeneration() -> std::uint64_t {
    return backend::BackendRuntime::GetInstance().GetAudioBackendGeneration();
}

auto SetAudioBackend(const backend::AudioBackendId id) -> bool {
    return backend::BackendRuntime::GetInstance().SetAudioBackend(id);
}

auto AddAudioBackendListener(backend::IAudioBackendChangeListener& listener) -> void {
    backend::BackendRuntime::GetInstance().AddAudioListener(listener);
}

auto RemoveAudioBackendListener(backend::IAudioBackendChangeListener& listener) -> void {
    backend::BackendRuntime::GetInstance().RemoveAudioListener(listener);
}

auto GetAudioDecoders() -> AudioDecoderRegistry& {
    return backend::BackendRuntime::GetInstance().AudioDecoders();
}

} // namespace atom::audio

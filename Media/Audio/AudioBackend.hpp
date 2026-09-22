// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file AudioBackend.hpp
 * @brief Product-level audio backend selection (wraps BackendRuntime).
 *
 * Gameplay and app code should use this header instead of
 * <Backend/Runtime/BackendRuntime.hpp>. Advanced backend/decoder authoring
 * still goes through BackendRuntime (see Backend/Runtime/README-CN.md).
 */

#ifndef ATOM_MEDIA_AUDIO_AUDIO_BACKEND_HPP
#define ATOM_MEDIA_AUDIO_AUDIO_BACKEND_HPP

#include <cstdint>
#include <string>

#include <Backend/Contracts/Audio/AudioBackendId.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>

namespace atom::backend {
class IAudioBackendChangeListener;
}

namespace atom::audio {

// Canonical id of the active playback backend (e.g. "sdl3").
[[nodiscard]] auto GetAudioBackendId() -> const std::string&;

// Monotonic switch counter — stamp cached sources with this and re-resolve
// after a change.
[[nodiscard]] auto GetAudioBackendGeneration() -> std::uint64_t;

// Switch the global playback backend. Failed switches leave the active
// backend untouched. Playback does not migrate; callers must Load/Play again.
auto SetAudioBackend(backend::AudioBackendId id) -> bool;

auto AddAudioBackendListener(backend::IAudioBackendChangeListener& listener) -> void;
auto RemoveAudioBackendListener(backend::IAudioBackendChangeListener& listener) -> void;

// Global format-decoder registry (settings / diagnostics UI).
[[nodiscard]] auto GetAudioDecoders() -> AudioDecoderRegistry&;

} // namespace atom::audio

#endif // ATOM_MEDIA_AUDIO_AUDIO_BACKEND_HPP
